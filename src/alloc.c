/**
 * file: alloc.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

/* System includes */
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <io/nw.h>
#include <msg.h>
#include <util/queue.h>
#include <debug.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

/* Internal state */

static struct queue work_q; /* work requests (messages) go here */

static struct message_forward nw_forward;

static pthread_t handler_tid;
static bool handler_alive = false;

/* Private functions */

/* TODO message handlers */

static int
process_msg(struct message *m)
{
    /*
     * type      status  action
     * ------------------------
     * REQ_ALLOC REQ     find mem, send reply (status->resp)
     * REQ_ALLOC RESP    new msg, do_alloc/req, send to rank
     * DO_ALLOC  REQ     call libRMA, send reply (status->resp)
     * DO_ALLOC  RESP    insert to MQ to return to app
     * DO_FREE   REQ     libRMA, send reply (chg status)
     * DO_FREE   RESP    depends who sent request..
     *                      i) process explicitly requested free
     *                      ii) process died, system requested free
     */
    printd("got a message: type=%s status=%s pid=%d rank=%d\n",
            MSG_TYPE2CHAR(m->type), MSG_STATUS2CHAR(m->status),
            m->pid, m->rank);
    return 0;
}

/* other modules submit messages to us via this function */
static int
import_msg(struct queue *ignored, struct message *m, ...)
{
    q_push(&work_q, m);
    return 0;
}

static int
export_msg(struct message *m)
{
    int retval = 0;
    if (nw_forward.handle) nw_forward.handle(nw_forward.q, m);
    else
    {
        printd("Error exporting alloc msg - null forward handle\n");
        retval = -1;
    }
    return retval;
}

static void
queue_handler_cleanup(void *arg)
{
    handler_alive = false;
}

static void *
queue_handler(void *arg)
{
    int pstate;
    struct message msg;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &pstate);
    pthread_cleanup_push(queue_handler_cleanup, NULL);

    handler_alive = true;

    printd("mem thread alive\n");

    while (true) /* main loop */
    {
        /* empty the work queue */
        while (!q_empty(&work_q))
        {
            if (q_pop(&work_q, &msg) == 0)
            {
                if (process_msg(&msg) < 0)
                    printd("Error processing message\n");
            }
            else
                printd("q_pop returned fail\n");
        }
        usleep(500);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

/* Public functions */

int
mem_init(void)
{
    struct message_forward f = { .handle = import_msg, .q = NULL };

    printd("memory interface initializing\n");

    q_init(&work_q, sizeof(struct message));

    if (nw_init() < 0)
        return -1;
    nw_forward = nw_get_import();
    nw_set_export(&f);

    /* TODO collect memory information about node */

    return 0;
}

int
mem_launch(void)
{
    int err;

    if (nw_launch() < 0)
        return -1;

    printd("memory interface launching worker thread\n");

    if (handler_alive) return -1;
    err = pthread_create(&handler_tid, NULL, queue_handler, NULL);
    if (err < 0) return -1;
    while (!handler_alive) ;

    /* TODO inject messages containing static configuration of node (regarding
     * memory capacity, etc) and send to all other ranks. expect no reply  */

    return 0;
}

/* handoff message to network work queue */
int
mem_new_request(struct message *m)
{
    int err = -1;
    printd("new message\n");
    if (nw_forward.handle)
        err = nw_forward.handle(nw_forward.q, m, 0/*rank*/);
    return err;
}

int
mem_set_export(struct message_forward *f)
{
    if (!f) return -1;
    nw_forward.handle = f->handle;
    nw_forward.q = f->q;
    return 0;
}

struct message_forward
mem_get_import(void)
{
    struct message_forward f = { .handle = import_msg, .q = NULL };
    return f;
}

void
mem_fin(void)
{
    nw_fin();

    if (!handler_alive) return;
    printd("memory interface finalizing\n");
    if (0 == pthread_cancel(handler_tid))
        pthread_join(handler_tid, NULL);
    while (handler_alive) ;
    q_free(&work_q);
}
