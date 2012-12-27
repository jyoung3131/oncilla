/**
 * file: main.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: daemon process. some code borrowed from Shadowfax project
 *
 * TODO
 *  - thread to monitor processes. if gone, 'disconnect' them
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <debug.h>
#include <io/nw.h>
#include <mem.h>
#include <pmsg.h>

/* Directory includes */

/* Definitions */

struct app
{
    struct list_head link;
    pid_t pid;
};

#define for_each_app(app, apps) \
    list_for_each_entry(app, &apps, link)

#define lock_apps()     pthread_mutex_lock(&apps_lock)
#define unlock_apps()   pthread_mutex_unlock(&apps_lock)

/* Globals */

static LIST_HEAD(apps); /* connecting processes on local node */
static pthread_mutex_t apps_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t poll_tid;

/* queue of messages mem wants to sent out to processes */
static struct queue outbox;

/* Functions */

/* do something with in-coming messages from apps */
static void
process_msg(struct message *msg)
{
    struct app *app = NULL;

    switch (msg->type) {

    case MSG_CONNECT:
    {
        app = calloc(1, sizeof(*app));
        ABORT2(!app);
        INIT_LIST_HEAD(&app->link);
        app->pid = msg->pid;

        lock_apps();
        list_add(&app->link, &apps);
        unlock_apps();

        if (pmsg_attach(app->pid) < 0) {
            fprintf(stderr, "error attaching new pid %d\n", app->pid);
            return;
        }

        msg->type = MSG_CONNECT_CONFIRM;
        msg->status = MSG_RESPONSE;
        pmsg_send(app->pid, msg);

    }
    break;

    case MSG_DISCONNECT:
    {
        printd("app %d departing\n", msg->pid);
        lock_apps();
        for_each_app(app, apps)
            if (msg->pid == app->pid)
                break;
        BUG(!app);
        list_del(&app->link);
        unlock_apps();

        printd("app %d found, detaching\n", msg->pid);
        pmsg_detach(app->pid);
        free(app);
    }
    break;

    /* all other messages */
    default:
    {
        msg->rank = nw_get_rank();
        mem_add_msg(msg);
    }
    break;

    }
}

static void *
poll_mailbox(void *arg)
{
    struct message msg;

    printd("mailbox poller alive\n");

    while (true) {
        while (!q_empty(&outbox)) {
            q_pop(&outbox, &msg);
            pmsg_send(msg.pid, &msg);
        }
        while (pmsg_pending() > 0) {
            if (pmsg_recv(&msg, false) < 0)
                pthread_exit(NULL);
            printd("got a msg: %d\n", msg.type);
            process_msg(&msg);
        }
        usleep(500);
    }

    return NULL;
}

static int
launch_poll_thread(void)
{
    if (pthread_create(&poll_tid, NULL, poll_mailbox, NULL) < 0) {
        printd("error launching mailbox polling thread\n");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    printd("Verbose printing enabled\n");

    q_init(&outbox, sizeof(struct message));

    if (mem_init() < 0) {
        fprintf(stderr, "error initializing mem\n");
        return -1;
    }

    /* mem will append msgs to apps into this queue */
    mem_set_outbox(&outbox);

    pmsg_cleanup();
    if (pmsg_init(sizeof(struct message)) < 0) {
        fprintf(stderr, "error initializing pmsg\n");
        return -1;
    }
    if (pmsg_open(PMSG_DAEMON_PID) < 0) {
        fprintf(stderr, "error opening recv mailbox\n");
        return -1;
    }
    if (launch_poll_thread() < 0) {
        fprintf(stderr, "error launching poll thread\n");
        return -1;
    }

    if (mem_launch() < 0) {
        fprintf(stderr, "error launching\n");
        return -1;
    }

    /* TODO Need to wait on signal or something instead of sleeping */
    sleep(3600);

    mem_fin();
    return 0;
}
