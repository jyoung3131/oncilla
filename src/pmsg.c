/**
 * file: pmsg.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: interface for processes to send each other messages via 'mailboxes'
 * (POSIX message queues). mailboxes exist per process and are receive-only.
 * sending a message requires attaching to another process' mailbox.
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
#include <pmsg.h>
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

//! Default priority for messages
#define MQ_DFT_PRIO				0

#define for_each_mq(mq, mqs) \
    list_for_each_entry(mq, &mqs, link)

#define lock_mailboxes()      pthread_mutex_lock(&mailboxes_lock)
#define unlock_mailboxes()    pthread_mutex_unlock(&mailboxes_lock)

struct mailbox
{
    struct list_head link;
    char name[MAX_LEN];
    pid_t pid;
    mqd_t id;
};

/* Internal mb */

static LIST_HEAD(mailboxes);
static pthread_mutex_t mailboxes_lock = PTHREAD_MUTEX_INITIALIZER;

static struct mailbox recv_mb;
static struct mailbox *daemon_mb;

static size_t max_msg_size = 0UL;

/* Private functions */

static struct mailbox *
__find_mailbox(pid_t pid)
{
    struct mailbox *mb = NULL;
    for_each_mq(mb, mailboxes)
        if (pid == mb->pid)
            break;
    return mb;
}

static void
__rm_mailbox(struct mailbox *mb)
{
    list_del(&mb->link);
}

static struct mailbox *
find_mailbox(pid_t pid)
{
    struct mailbox *mb = NULL;
    lock_mailboxes();
    mb = __find_mailbox(pid);
    unlock_mailboxes();
    return mb;
}

static void
rm_mailbox(struct mailbox *mb)
{
    lock_mailboxes();
    __rm_mailbox(mb);
    unlock_mailboxes();
}

static void
add_mailbox(struct mailbox *mb)
{
    lock_mailboxes();
    list_add(&mailboxes, &mb->link);
    unlock_mailboxes();
}

// Receive a message. If we are sent some signal while receiving, we retry the
// receive. If the MQ is empty, exit with -EAGAIN
static int
recv_message(struct mailbox *mb, void *msg)
{
	int err, exit_errno;
again:
	err = mq_receive(mb->id, (char*)msg, max_msg_size, NULL);
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
recv_message_block(struct mailbox *mb, void *msg)
{
	int err, exit_errno;
again:
	err = mq_receive(mb->id, (char*)msg, max_msg_size, NULL);
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
open_other_mb(struct mailbox *mb)
{
    sprintf(mb->name, "%s%d", ATTACH_NAME_PREFIX, mb->pid);
    printd("mq name '%s'\n", mb->name);
    mb->id = mq_open(mb->name, MQ_OPEN_CONNECT_FLAGS);
    if (!MQ_ID_IS_VALID(mb->id)) {
        perror("mq_open on incoming process");
        return -1;
    }
    return 0;
}

static int
close_other_mb(struct mailbox *mb)
{
    int err;
    printd("mq name '%s'\n", mb->name);
    err = mq_close(mb->id);
    if (err < 0) {
        perror("mq_close on departing process");
        return -1;
    }
    return 0;
}

static int
attach_daemon(void)
{
    struct mq_attr qattr;

    memset(&qattr, 0, sizeof(qattr));
	qattr.mq_maxmsg = MQ_MAX_MESSAGES;
	qattr.mq_msgsize = max_msg_size;

    daemon_mb = calloc(1, sizeof(*daemon_mb));
    if (!daemon_mb) {
        printd("out of memory\n");
        return -1;
    }
    snprintf(daemon_mb->name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
    daemon_mb->pid = PMSG_DAEMON_PID;
    daemon_mb->id = mq_open(daemon_mb->name, MQ_OPEN_CONNECT_FLAGS, MQ_PERMS, qattr);
    if (!MQ_ID_IS_VALID(daemon_mb->id)) {
        perror("mq_open");
        return -1;
    }
    return 0;
}

static int
detach_daemon(void)
{
    if (0 > mq_close(daemon_mb->id)) {
        perror("mq_close");
        return -1;
    }
#if 0
    if (0 > mq_unlink(daemon_mb->name)) {
        perror("mq_unlink");
        return -1;
    }
    if (0 > mq_close(daemon_mb->id)) {
        perror("mq_close");
        return -1;
    }
#endif
    return 0;
}

/* retries if MQ is full */
static int
send_message(struct mailbox *mb, void *msg)
{
	int err, exit_errno;
again:
	err = mq_send(mb->id, (char*)msg, max_msg_size, MQ_DFT_PRIO);
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

#if 0
static void *
queue_handler(void *arg)
{
    int pstate, err;
    struct message u_msg;
    struct mq_message mq_msg;
    struct mailbox *mq = NULL;

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

            mq = find_mailbox(u_msg.pid);
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

static int
attach_send_connect(void)
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

static int
attach_send_disconnect(void)
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
#endif

/* Public functions */

int
pmsg_init(size_t pmsg_size)
{
    if (pmsg_size <= 0)
        return -1;
    max_msg_size = pmsg_size;
    return 0;
}

int
pmsg_open(pid_t self_pid)
{
    struct mq_attr qattr;

    memset(&qattr, 0, sizeof(qattr));
    qattr.mq_maxmsg = MQ_MAX_MESSAGES;
    qattr.mq_msgsize = max_msg_size;

    if (self_pid == PMSG_DAEMON_PID) {
        memset(&recv_mb, 0, sizeof(recv_mb));
        snprintf(recv_mb.name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
        recv_mb.id = mq_open(recv_mb.name,
                MQ_OPEN_OWNER_FLAGS, MQ_PERMS, &qattr);
        if (!MQ_ID_IS_VALID(recv_mb.id) ) {
            perror("mq_open");
            return -1;
        }
    } else {
        snprintf(recv_mb.name, MAX_LEN, "%s%d",
                ATTACH_NAME_PREFIX, self_pid);
        recv_mb.id = mq_open(recv_mb.name,
                MQ_OPEN_OWNER_FLAGS, MQ_PERMS, &qattr);
        if (!MQ_ID_IS_VALID(recv_mb.id)) {
            perror("mq_open");
            return -1;
        }
    }
    recv_mb.pid = self_pid;

    printd("opened receive mailbox\n");
    return 0;
}

int
pmsg_close(void)
{
    if (0 > mq_close(recv_mb.id)) {
        perror("mq_close");
        return -1;
    }
    printd("Closed recv mailbox %d\n", recv_mb.id);

    if (0 > mq_unlink(recv_mb.name)) {
        perror("mq_unlink");
        return -1;
    }
    printd("Unlinked recv mailbox '%s'\n", recv_mb.name);
    return 0;
}

int
pmsg_attach(pid_t to_pid)
{
    struct mailbox *mb = NULL;

    if (to_pid == PMSG_DAEMON_PID)
        return attach_daemon();

    mb = calloc(1, sizeof(*mb));
    if (!mb) {
        printd("out of memory\n");
        return -1;
    }
    INIT_LIST_HEAD(&mb->link);
    mb->pid = to_pid;
    if (open_other_mb(mb) < 0) {
        printd("open other mb for pid %d failed\n", to_pid);
        free(mb);
        return -1;
    }
    add_mailbox(mb);
    return 0;
}

int
pmsg_detach(pid_t to_pid)
{
    struct mailbox *mb = NULL;

    if (to_pid == PMSG_DAEMON_PID)
        return detach_daemon();

    lock_mailboxes();
    mb = __find_mailbox(to_pid);
    if (!mb) {
        unlock_mailboxes();
        printd("could not locate mailbox for pid %d\n", to_pid);
        return -1;
    }
    __rm_mailbox(mb);
    if (close_other_mb(mb) < 0) {
        unlock_mailboxes();
        free(mb);
        printd("error detaching from mailbox of pid %d\n", to_pid);
        return -1;
    }
    unlock_mailboxes();
    free(mb);
    return 0;
}

int
pmsg_send(pid_t to_pid, void *msg)
{
    struct mailbox *mb = NULL;

    if (to_pid == PMSG_DAEMON_PID)
        mb = daemon_mb;
    else {
        mb = find_mailbox(to_pid);
        if (!mb) {
            printd("pid %d not attached\n", to_pid);
            return -1;
        }
    }
    if (send_message(mb, msg) < 0) {
        printd("error sending message to pid %d\n", mb->pid);
        return -1;
    }
    return 0;
}

int
pmsg_recv(void *msg, bool block)
{
    if (!msg)
        return -1;
    if (block) {
        if (0 > recv_message_block(&recv_mb, msg)) {
            printd("error receiving message: %s\n", strerror(errno));
            return -1;
        }
    } else {
        if (0 > recv_message(&recv_mb, msg)) {
            printd("error receiving message\n");
            return -1;
        }
    }
    return 0;
}

int
pmsg_cleanup(void)
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

    snprintf(name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
    if (mq_unlink(name) < 0)
        perror("removing daemon mq");

    printf("> Cleaned %d lingering pmsg mailboxes\n", num_cleaned);

    return 0;

fail_close:
    if (fptr)
        fclose(fptr);
fail:
    return -1;
}

int pmsg_pending(void)
{
    struct mq_attr attr;
    mq_getattr(recv_mb.id, &attr);
    return attr.mq_curmsgs;
}

/*
 * *************************
 */

/* library functions */

#if 0
int attach_init(void)
{
	struct mq_attr qattr;

    memset(&qattr, 0, sizeof(qattr));
	qattr.mq_maxmsg = MQ_MAX_MESSAGES;
	qattr.mq_msgsize = max_msg_size;

    /* configure recv_mq; daemon writes to this */
    recv_mq.pid = -1; /* only daemon code uses this field */
    snprintf(recv_mq.name, MAX_LEN, "%s%d", ATTACH_NAME_PREFIX, getpid());
    recv_mq.id = mq_open(recv_mq.name, MQ_OPEN_OWNER_FLAGS, MQ_PERMS, &qattr);
    if (!MQ_ID_IS_VALID(recv_mq.id)) {
        perror("mq_open");
        return -1;
    }

    /* configure send_mq; we attach to give daemon messages */
    snprintf(send_mq.name, MAX_LEN, "%s", ATTACH_DAEMON_MQ_NAME);
    send_mq.pid = -1;
    send_mq.id = mq_open(send_mq.name, MQ_OPEN_CONNECT_FLAGS, MQ_PERMS, qattr);
    if (!MQ_ID_IS_VALID(send_mq.id)) {
        perror("mq_open");
        return -1;
    }

    if (0 > attach_send_connect()) {
        printd("error sending connect to daemon\n");
        return -1;
    }

    return 0;
}

int attach_tini(void)
{
    if (0 > attach_send_disconnect()) {
        printd("error sending disconnect to daemon\n");
        return -1;
    }

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

/* TODO Fix this to send an allocation request */
int attach_send_request(struct mailbox *recv_mq, struct mailbox *send,
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

/* send message to daemon */
int attach_send_daemon(struct message *u_msg)
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
#endif
