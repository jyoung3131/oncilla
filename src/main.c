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
#include <mq.h>

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
static struct message_forward mq_forward;

/* Functions */

/* this function handles incoming messages from the app; considered the 'export'
 * path for the mq module */
static void process_app_msg(msg_event e, pid_t pid, void *data)
{
    int err;
    struct app *app = NULL;

    switch (e) {

    case OCM_CONNECT:
    {
        printd("got OCM_CONNECT from pid %d\n", pid);

        app = calloc(1, sizeof(*app));
        if (app) {
            INIT_LIST_HEAD(&app->link);
            app->pid = pid;

            lock_apps();
            list_add(&app->link, &apps);
            unlock_apps();

            err = attach_allow(pid);
            if (err < 0)
                fprintf(stderr, "Error attaching PID %d\n", pid);
            err = attach_send_allow(pid, true);
            if (err < 0)
                fprintf(stderr, "Error notifying attach to PID %d\n", pid);
        }
        else
            fprintf(stderr, "Out of memory\n");
    }
    break;

    case OCM_DISCONNECT:
    {
        printd("got OCM_DISCONNECT from pid %d\n", pid);

        lock_apps();
        for_each_app(app, apps)
            if (app->pid == pid)
                break;
        if (app)
            list_del(&app->link);
        unlock_apps();

        BUG(!app);

        if (0 > attach_dismiss(pid))
            fprintf(stderr, "Error dismissing PID %d\n", pid);
        free(app);
    }
    break;

    case OCM_USER:
    {
        printd("got OCM_USER from pid %d\n", pid);

        /* such messages could be new requests (e.g. allocate, free) or response to
         * an allocation that is in-progress (e.g. mem module telling app to
         * allocate memory and register with RMA interface)
         */
        struct message u_msg = *((struct message*)data);
        /* fill in remaining fields */
        u_msg.rank = nw_get_rank();
        u_msg.u.req.orig_rank = u_msg.rank;
        if (0 > mem_umsg_recv(&u_msg)) {
            fprintf(stderr, "error transferring msg to nw\n");
            printd("error transferring msg to nw\n");
        }
    }
    break;

    default:
    break;
    }
}

int main(int argc, char *argv[])
{
    printd("Verbose printing enabled\n");

    ABORT2(mem_init() < 0); /* mem and io interfaces */
    ABORT2(attach_open(process_app_msg) < 0); /* mq interface */
    mq_forward = attach_get_import();
    ABORT2(mem_set_export(&mq_forward) < 0); /* connect mq and mem */

    ABORT2(mem_launch() < 0); /* start system */

    /* TODO Need to wait on signal or something instead of sleeping */
    sleep(10);

    mem_fin();
    return 0;
}
