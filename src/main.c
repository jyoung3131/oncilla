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
#include <io/nw.h>
#include <mem.h>
#include <mq.h>
#include <debug.h>

/* Directory includes */

/* Definitions */

struct app
{
    struct list_head link;
    pid_t pid;
    struct mq_state mq; /* PID's recv'ing MQ state */
};

#define for_each_app(app, apps) \
    list_for_each_entry(app, &apps, link)

#define lock_apps()     pthread_mutex_lock(&apps_lock)
#define unlock_apps()   pthread_mutex_unlock(&apps_lock)

/* Globals */

static LIST_HEAD(apps); /* connecting processes on local node */
static pthread_mutex_t apps_lock = PTHREAD_MUTEX_INITIALIZER;

/* Functions */

static void process_app_msg(msg_event e, pid_t pid, void *data)
{
    int err;
    struct app *app = NULL;

    switch (e) {

    case OCM_CONNECT:
    {
        printd("got ocm_connect msg from pid %d\n", pid);
        app = calloc(1, sizeof(*app));
        if (app) {
            INIT_LIST_HEAD(&app->link);
            app->pid = pid;

            lock_apps();
            list_add(&app->link, &apps);
            unlock_apps();

            err = attach_allow(&app->mq, pid);
            if (err < 0)
                fprintf(stderr, "Error attaching PID %d\n", pid);

            err = attach_send_allow(&app->mq, true);
            if (err < 0)
                fprintf(stderr, "Error notifying attach to PID %d\n", pid);
        }
        else
            fprintf(stderr, "Out of memory\n");
    }
    break;

    case OCM_DISCONNECT:
    {
        printd("got ocm_disconnect msg from pid %d\n", pid);

        lock_apps();
        for_each_app(app, apps)
            if (app->pid == pid)
                break;
        if (app)
            list_del(&app->link);
        unlock_apps();

        if (app) {
            err = attach_dismiss(&app->mq);
            if (err < 0)
                fprintf(stderr, "Error dismissing PID %d\n", pid);
            free(app);
        }
        else
            fprintf(stderr, "Fatal error, PID %d leaving but"
                    " state not found\n", pid);
    }
    break;

#if 0 /* to test sending a message without the MQ module or apps */
    struct message m;
    m.type = MSG_REQ_ALLOC;
    m.status = MSG_REQUEST;
    m.pid = getpid();
    m.rank = nw_get_rank();
    m.u.req.orig_rank = m.rank;
    m.u.req.bytes = (4 << 10);
    if (m.rank > 0)
        if (mem_new_request(&m) < 0)
            printd("Error submitting new req\n");
#endif

    default:
    break;
    }
}

int main(int argc, char *argv[])
{
    int err;

    printd("Verbose printing enabled\n");

    /* The mem interface will start the IO inteface. */
    ABORT2(mem_init() < 0);

    /* TODO Start the MQ, and alloc interfaces. */
    err = attach_open(process_app_msg);
    if (err < 0) {
        fprintf(stderr, "Error opening MQ interface\n");
        return -1;
    }

    /* TODO connect mem export with mq import */

    ABORT2(mem_launch() < 0);

    /* TODO Need to wait on signal or something instead of sleeping */
    sleep(10);

    mem_fin();
    return 0;
}
