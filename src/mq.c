/**
 * file: mq.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: message queue interface. file borrowed from Shadowfax project.
 *
 * TODO Could split this file into two parts, mq_daemon.c and mq_client.c to
 * make it clear which internal state is maintained with which interface.
 */

/* System includes */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <debug.h>
#include <mq.h>
#include <msg.h>
#include <util/queue.h>

/* Definitions */

/* The permissions settings are masked against the process umask. */
#define MQ_PERMS				(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | \
														S_IROTH | S_IWOTH)
#define MQ_OPEN_OWNER_FLAGS		(O_RDONLY | O_CREAT | O_EXCL | O_NONBLOCK)
#define MQ_OPEN_CONNECT_FLAGS	(O_WRONLY)

#define MQ_ID_INVALID_VALUE		((mqd_t) - 1) // RTFM
#define MQ_ID_IS_VALID(m)		((m) != MQ_ID_INVALID_VALUE)

#define MQ_MAX_MESSAGES			8

//! Maximum message size allowable. Just set to the size of our structure.
#define MQ_MAX_MSG_SIZE			(sizeof(struct mq_message))

//! Default priority for messages
#define MQ_DFT_PRIO				0

#define for_each_mq(mq, mqs) \
    list_for_each_entry(mq, &mqs, link)

#define lock_mqs()      pthread_mutex_lock(&mq_states_lock)
#define unlock_mqs()    pthread_mutex_unlock(&mq_states_lock)

struct mq_message
{
    msg_event type;
    pid_t pid; /* identify application process */
    union {
        bool allow; /* OCM_CONNECT_ALLOW */
        struct message u_msg; /* OCM_USER user message type */
    } m; /* actual message data */
};

struct mq_state
{
    struct list_head link;
    bool valid;
    char name[MAX_LEN];
    pid_t pid;
    mqd_t id;
    msg_recv_callback notify;
};

/* Internal state */

static struct queue msg_q;
static struct mq_state daemon_mq; /* interface assumes one daemon MQ */
static LIST_HEAD(mq_states);
static pthread_mutex_t mq_states_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t handler_tid;
/* volatile: gcc optimizes out updates to variable across threads */
static volatile bool handler_alive = false;

/* state for library-side of interface */
static struct mq_state send_mq, recv_mq;

/* Private functions */

static struct mq_state *
find_mq(pid_t pid)
{
    struct mq_state *mq = NULL;
    lock_mqs();
    for_each_mq(mq, mq_states)
        if (pid == mq->pid)
            break;
    unlock_mqs();
    return mq;
}

/* forward declaration */
static void __process_messages(union sigval sval);

static inline int
set_notify(struct mq_state *state)
{
	struct sigevent event;
	memset(&event, 0, sizeof(event));
	event.sigev_notify = SIGEV_THREAD;
	event.sigev_notify_function = __process_messages;
	event.sigev_value.sival_ptr = state;
	if ( 0 > mq_notify(state->id, &event) )
    {
        perror("mq_notify");
        return -1;
    }
	return 0;
}

// Receive a message. If we are sent some signal while receiving, we retry the
// receive. If the MQ is empty, exit with -EAGAIN
static int
recv_message(struct mq_state *state, struct mq_message *msg)
{
	int err, exit_errno;
again:
	err = mq_receive(state->id, (char*)msg, sizeof(*msg), NULL);
	if (err < 0) {
		if (errno == EINTR)
			goto again; // A signal interrupted the call, try again
		exit_errno = -(errno);
		goto fail;
	}
	return 0;
fail:
	return exit_errno;
}

// Same as recv_message except if the MQ is empty, keep retrying
static int
recv_message_block(struct mq_state *state, struct mq_message *msg)
{
	int err, exit_errno;
again:
	err = mq_receive(state->id, (char*)msg, sizeof(*msg), NULL);
	if (err < 0) {
		if (errno == EINTR)
			goto again; // A signal interrupted the call, try again
		if (errno == EAGAIN)
			goto again; // retry if empty
		exit_errno = -(errno);
		goto fail;
	}
	return 0;
fail:
	return exit_errno;
}

/* used by daemon to open the app MQ when it connects */
static int
open_other_mq(struct mq_state *state)
{
    sprintf(state->name, "%s%d", ATTACH_NAME_PREFIX, state->pid);
    printd("mq name '%s'\n", state->name);
    state->id = mq_open(state->name, MQ_OPEN_CONNECT_FLAGS);
    if (!MQ_ID_IS_VALID(state->id)) {
        perror("mq_open on incoming process");
        return -1;
    }
    return 0;
}

static int
close_other_mq(struct mq_state *state)
{
    int err;
    printd("mq name '%s'\n", state->name);
    err = mq_close(state->id);
    if (err < 0) {
        perror("mq_close on departing process");
        return -1;
    }
    return 0;
}

/* called by the mqueue layer on incoming messages */
static void
process_messages(struct mq_state *state)
{
    int err;
    struct mq_message msg;

    if (!state || !state->valid) {
        printd("!state || !state->valid\n");
        return;
    }

    if (!MQ_ID_IS_VALID(state->id))
        return;

	// Re-enable notification on the message queue. The man page says
	// notifications are one-shot, and need to be reset after each trigger. It
	// additionally says that notifications are only triggered upon receipt of a
	// message on an EMPTY queue. Thus we set notification first, then empty the
	// queue completely.
    err = set_notify(state);
    if (err < 0) {
		fprintf(stderr, "Error setting notify on PID %d: %s\n",
				state->pid, strerror(-(err)));

		return;
	}

    while ( 1 ) {
        err = recv_message(state, &msg);
        if ( err < 0 ) {
            if (err == -EAGAIN)
                break; // MQ is empty
            fprintf(stderr, "Error recv msg on id %d: %s\n",
                    state->id, strerror(-(err)));
            return;
        }

        state->notify(msg.type, msg.pid, (void *)&msg.m);
    }
}

static void
__process_messages(union sigval sval)
{
    struct mq_state *state;
    /* Extract our specific state from the argument */
    state = (struct mq_state*) sval.sival_ptr;
    process_messages(state);
}

/* retries if MQ is full */
static int
send_message(struct mq_state *state, struct mq_message *msg)
{
	int err, exit_errno;
again:
	err = mq_send(state->id, (char*)msg, sizeof(*msg), MQ_DFT_PRIO);
	if (err < 0) {
		if (errno == EINTR)
			goto again; // A signal interrupted the call, try again
		if (errno == EAGAIN)
			goto again; // MQ is full, try again (spin)
		exit_errno = -(errno);
		goto fail;
	}
	return 0;
fail:
	return exit_errno;
}

static int
import_msg(struct queue *ignored, struct message *m, ...)
{
    q_push(&msg_q, m);
    return 0;
}

static void
queue_handler_cleanup(void *arg)
{
    handler_alive = false;
}

/* mem interface gives us (mq interface) messages to send back to applications.
 * here we sift through the work queue and demultiplex these messages out
 */
static void *
queue_handler(void *arg)
{
    int pstate, err;
    struct message u_msg;
    struct mq_message mq_msg;
    struct mq_state *mq = NULL;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &pstate);
    pthread_cleanup_push(queue_handler_cleanup, NULL);

    handler_alive = true;

    printd("mq thread alive\n");

    while (true)
    {
        while (!q_empty(&msg_q))
        {
            err = q_pop(&msg_q, &u_msg);
            BUG(err != 0);

            mq = find_mq(u_msg.pid);
            BUG(!mq);

            mq_msg.type = OCM_USER;
            err = send_message(mq, &mq_msg);
            BUG(err != 0);
        }
        usleep(500);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

static int
launch_worker(void)
{
    int err;
    if (handler_alive) return -1;
    printd("launching mq worker\n");
    err = pthread_create(&handler_tid, NULL, queue_handler, NULL);
    if (err < 0) return -1;
    while (!handler_alive) ;
    return 0;
}

/* Public functions */

/* daemon functions */

/* Searches for all MQs previous failed launches may have left behind and
 * unlinks them. Do NOT call this function AFTER initializing the MQ interface
 * as it will remove the daemon's MQ. */
int attach_clean(void)
{
    char pidmax_path[] = "/proc/sys/kernel/pid_max";
    int maxpid;
    FILE *fptr = NULL;
    char line[32]; /* XXX 32 chosen arbitrarily */

    int pid, num_cleaned = 0;
    char name[MAX_LEN];

    /* obtain system setting for maximum PID */
    if (0 > access(pidmax_path, R_OK))
        goto fail;
    fptr = fopen(pidmax_path, "r");
    if (!fptr)
        goto fail;
    if (NULL == fgets(line, 32, fptr))
        goto fail_close;
    fclose(fptr);
    fptr = NULL;
    maxpid = atoi(line);
    printd("maxpid=%d\n", maxpid);

    /* 1 is init, MQ will never exist */
    for (pid = 2; pid <= maxpid; pid++) {
        snprintf(name, MAX_LEN, "%s%d", ATTACH_NAME_PREFIX, pid);
        if (0 > mq_unlink(name)) {
            if (errno == ENOENT)
                continue;
            fprintf(stderr, "> Error unlinking MQ %s: %s\n",
                    name, strerror(errno));
        }
        num_cleaned++;
    }
    printf("> Cleaned %d stray message queues\n", num_cleaned);

    return 0;

fail_close:
    if (fptr)
        fclose(fptr);
fail:
    return -1;
}

int attach_open(msg_recv_callback notify)
{
    struct mq_attr qattr;
    bool tried_again = false;

    if ( !notify )
        return -1;

    memset(&daemon_mq, 0, sizeof(daemon_mq));
    daemon_mq.notify = notify;
    snprintf(daemon_mq.name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
    daemon_mq.pid = -1; /* not used for open() */
    daemon_mq.valid = true;

    memset(&qattr, 0, sizeof(qattr));
    qattr.mq_maxmsg = MQ_MAX_MESSAGES;
    qattr.mq_msgsize = MQ_MAX_MSG_SIZE;
try_again:
    daemon_mq.id =
        mq_open(daemon_mq.name, MQ_OPEN_OWNER_FLAGS, MQ_PERMS, &qattr);
    if ( !MQ_ID_IS_VALID(daemon_mq.id) ) {
        if ( errno == EEXIST ) {
            fprintf(stderr, "> Daemon already running in another instance,"
                    " or previously crashed and old MQ was not cleaned up\n"
                    "> Removing MQ '%s'\n", daemon_mq.name);
            if (0 > mq_unlink(daemon_mq.name)) {
                // try to remove it and start again
                fprintf(stderr, "> Failed to remove MQ. Aborting\n");
                abort();
            } else {
                if (tried_again) {
                    fprintf(stderr, "> Failed to open MQ after removal."
                            " Aborting\n");
                    abort();
                }
                tried_again = true;
                goto try_again;
            }
        } else {
            return -1; /* some other error */
        }
    }
    printd("Opened daemon MQ %d '%s'\n",
            daemon_mq.id, daemon_mq.name);

    q_init(&msg_q, sizeof(struct message));
    if (0 > launch_worker())
        return -1;

    set_notify(&daemon_mq);
    return 0;
}

int attach_close(void)
{
    /* If the mq is first closed, the notify thread will somehow wake up and try
     * to do work. So we only unlink it. */
#if 0
    daemon_mq.valid = false;
    if (0 > mq_close(daemon_mqid)) {
        perror("mq_close");
        return -1;
    }
    printd("Closed daemon MQ %d\n", daemon_mq.id);
#endif

    if (0 > mq_unlink(daemon_mq.name)) {
        perror("mq_unlink");
        return -1;
    }
    printd("Unlinked daemon MQ '%s'\n", daemon_mq.name);
    return 0;
}

int attach_allow(pid_t pid)
{
    struct mq_state *state = find_mq(pid);
    BUG(!state);
    state->pid = pid;
    return open_other_mq(state);
}

int attach_dismiss(pid_t pid)
{
    struct mq_state *state = find_mq(pid);
    BUG(!state);
    return close_other_mq(state);
}

int attach_send_allow(pid_t pid, bool allow)
{
    struct mq_state *state = find_mq(pid);
    struct mq_message msg;

    BUG(!state);
    msg.type = OCM_CONNECT_ALLOW;
    msg.m.allow = allow;
    if (0 > send_message(state, &msg)) {
        fprintf(stderr, "Error sending message to PID %d\n", state->pid);
        return -1;
    }
    return 0;
}

struct message_forward attach_get_import(void)
{
    struct message_forward f = { .handle = import_msg, .q = NULL };
    return f;
}

#if 0
int attach_send_assembly(struct mq_state *state, assembly_key_uuid key)
{
    struct mq_message msg;
    msg.type = OCM_ASSIGN_ASSEMBLY;
    memcpy(msg.m.key, key, sizeof(assembly_key_uuid));
    if (0 > send_message(state, &msg)) {
        fprintf(stderr, "Error sending message to PID %d\n", state->pid);
        return -1;
    }
    return 0;
}
#endif

/* library functions */

int attach_init(void)
{
	struct mq_attr qattr;

    memset(&qattr, 0, sizeof(qattr));
	qattr.mq_maxmsg = MQ_MAX_MESSAGES;
	qattr.mq_msgsize = MQ_MAX_MSG_SIZE;

    recv_mq.notify = NULL; /* interposer recvs synchronously */
    recv_mq.pid = -1; /* only daemon code uses this field */
    snprintf(recv_mq.name, MAX_LEN, "%s%d", ATTACH_NAME_PREFIX, getpid());
    recv_mq.id = mq_open(recv_mq.name, MQ_OPEN_OWNER_FLAGS, MQ_PERMS, &qattr);
    if (!MQ_ID_IS_VALID(recv_mq.id)) {
        perror("mq_open");
        return -1;
    }

    snprintf(send_mq.name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
    send_mq.notify = NULL; /* send is an outbound mq */
    send_mq.pid = -1;
    send_mq.id = mq_open(send_mq.name, MQ_OPEN_CONNECT_FLAGS, MQ_PERMS, qattr);
    if (!MQ_ID_IS_VALID(send_mq.id)) {
        perror("mq_open");
        return -1;
    }

    return 0;
}

int attach_tini(void)
{
    if (0 > mq_close(recv_mq.id)) {
        perror("mq_close");
        return -1;
    }
    if (0 > mq_unlink(recv_mq.name)) {
        perror("mq_unlink");
        return -1;
    }

    if (0 > mq_close(send_mq.id)) {
        perror("mq_close");
        return -1;
    }

    return 0;
}

int attach_send_connect(void)
{
    struct mq_message msg;

    msg.type = OCM_CONNECT;
    msg.pid = getpid();
    if (0 > send_message(&send_mq, &msg)) {
        fprintf(stderr, "Error sending message to daemon\n");
        return -1;
    }

    printd("Waiting for daemon to reply\n");

    /* block until daemon sends the okay */
    if (0 > recv_message_block(&recv_mq, &msg)) {
        fprintf(stderr, "Error receving message from daemon\n");
        return -1;
    }
    if (msg.type == OCM_CONNECT_ALLOW) {
        if (!msg.m.allow) {
            fprintf(stderr, "Not allowed to connect to daemon\n");
            return -1;
        }
    } else {
        fprintf(stderr, "Unexpected message recieved: %d\n", msg.type);
        return -1;
    }
    return 0;
}

int attach_send_disconnect(void)
{
    struct mq_message msg;

    msg.type = OCM_DISCONNECT;
    msg.pid = getpid();
    if (0 > send_message(&send_mq, &msg)) {
        fprintf(stderr, "Error sending message to daemon\n");
        return -1;
    }

    /* no response expected */

    return 0;
}

/* TODO Fix this to send an allocation request */
#if 0
int attach_send_request(struct mq_state *recv_mq, struct mq_state *send,
        struct assembly_hint *hint, assembly_key_uuid key)
{
    struct mq_message msg;

    if (!recv_mq || !send)
        return -1;

    msg.type = OCM_REQUEST_ASSEMBLY;
    msg.pid = getpid(); /* so the runtime knows which return queue to use */
    memcpy(&msg.m.hint, hint, sizeof(*hint));
    if (0 > send_message(send, &msg)) {
        fprintf(stderr, "Error sending message to daemon\n");
        return -1;
    }

    /* block until daemon has exported assembly for us */
    if (0 > recv_message_block(recv_mq, &msg)) {
        fprintf(stderr, "Error receving message from daemon\n");
        return -1;
    }
    if (msg.type == OCM_ASSIGN_ASSEMBLY) {
        memcpy(key, msg.m.key, sizeof(assembly_key_uuid));
    } else {
        fprintf(stderr, "Unexpected message recieved: %d\n", msg.type);
        return -1;
    }
    return 0;
}
#endif

/* send message to daemon */
int attach_send(struct message *u_msg)
{
    struct mq_message mq_msg;
    mq_msg.type = OCM_USER;
    mq_msg.pid = getpid();
    memcpy(&mq_msg.m.u_msg, u_msg, sizeof(*u_msg));
    if (0 > send_message(&send_mq, &mq_msg)) {
        printd("error sending msg to daemon\n");
        return -1;
    }
    return 0;
}

/* receive message from daemon */
int attach_recv(struct message *u_msg)
{
    struct mq_message mq_msg;
    if (0 > recv_message(&recv_mq, &mq_msg)) {
        printd("error receiving msg from daemon\n");
        return -1;
    }
    memcpy(u_msg, &mq_msg.m.u_msg, sizeof(*u_msg));
    return 0;
}

void attach_cleanup(void)
{
    /* TODO figure out how to get a list of all mqueues that exist, match the
     * name to the prefix we have and remove. */
}
