/**
 * file: mem.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: module gluing a node to the cluster, handles/processes allocation
 * messages and protocol. main.c or MQ module are the client to this interface;
 * we are the client to the nw and alloc interfaces.
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
#include <alloc.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

/* Internal state */

static struct queue work_q; /* work requests (messages) go here */

static struct message_forward nw_forward;

static pthread_t handler_tid;
static bool handler_alive = false;

/* Private functions */

/* forward declaration */
static int export_msg(struct message *, int);

static int
process_req_alloc(struct message *m)
{
    if (m->status == MSG_REQUEST)
    {
        BUG(nw_get_rank() != 0);
        /* make request, copy result */
        struct alloc_ation a;
        printf("rank0 recv msg: alloc req\n");
        BUG(alloc_find(&m->u.req, &a) < 0);
        m->u.alloc = a;
        m->status = MSG_RESPONSE;
        export_msg(m, m->rank); /* return to origin */
    }
    /* TODO RESP: new msg DO_ALLOC, send to rank */
    else if (m->status == MSG_RESPONSE)
    {
        printf("rank%d recv msg: alloc resp, convert to do_alloc command\n",
                nw_get_rank());
        m->type = MSG_DO_ALLOC;
        m->status = MSG_REQUEST;
        export_msg(m, m->u.alloc.rank);
    }
    return 0;
}

static int
process_do_alloc(struct message *m)
{
    if (m->status == MSG_REQUEST)
    {
        printf("rank%d msg: do alloc req, libRMA/glib/cuda alloc\n",
                nw_get_rank());
        ABORT2(alloc_ate(&m->u.alloc) < 0);
        m->status = MSG_RESPONSE;
        export_msg(m, m->rank); /* return to origin */
    }
    /* TODO RESP: insert to MQ to return to app */
    else if (m->status == MSG_RESPONSE)
    {
        printf("rank%d msg: do alloc resp, return to app via MQ\n",
                nw_get_rank());
        m->status = MSG_EOL;
        /*export_msg(m, m->pid);*/ /* return to app via MQ */
    }
    m->status++;
    return 0;
}

static int
process_do_free(struct message *m)
{
    /* TODO REQ: call libRMA/etc, send reply (status to response) */
    if (m->status == MSG_REQUEST)
    {
        printf("rank%d msg: do free req, libRMA/glib/cuda free\n",
                nw_get_rank());
    }
    /* TODO RESP: depends who sent request ...
     *              i) process explicitly requested free: to MQ
     *              ii) process died, drop this message
     */
    else if (m->status == MSG_RESPONSE)
    {
        printf("rank%d msg: do free resp, something\n",
                nw_get_rank());
    }
    m->status++;
    return 0;
}

/* other modules submit messages to us via this function */
static int
import_msg(struct queue *ignored, struct message *m, ...)
{
    q_push(&work_q, m);
    return 0;
}

/* for us to export to another module, like a message demuxer */
static int
export_msg(struct message *m, int dest_id/*entity as rank or pid*/)
{
    int retval = 0;

    /* mem_new_request handles these messages, nothing should generate them */
    BUG(m->type == MSG_REQ_ALLOC && m->status == MSG_REQUEST);

    if (m->status == MSG_REQUEST || m->status == MSG_RESPONSE)
    {
        /* always send responses back to the originating rank process */
        if (!nw_forward.handle ||
                nw_forward.handle(nw_forward.q, m, dest_id/*rank*/) < 0)
            BUG(1);
    }
    else if (m->status == MSG_EOL)
    {
        /* TODO export to MQ interface */
        //if (!mq_forward.handle ||
                //mq_forward.handle(mq_forward.q, m, dest_id/*pid*/) < 0)
        BUG(1);
    }
    else
    {
        BUG(1);
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
    int pstate, err;
    struct message msg;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &pstate);
    pthread_cleanup_push(queue_handler_cleanup, NULL);

    handler_alive = true;

    printd("mem thread alive\n");

    while (true)
    {
        while (!q_empty(&work_q))
        {
            if (q_pop(&work_q, &msg) != 0)
                BUG(1);

            err = 0;
            if (msg.type == MSG_REQ_ALLOC)
                err = process_req_alloc(&msg);
            else if (msg.type == MSG_DO_ALLOC)
                err = process_do_alloc(&msg);
            else if (msg.type == MSG_DO_FREE)
                err = process_do_free(&msg);
            else
                err = -1; /* unknown message, just die */

            if (err != 0)
                BUG(1);
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

/* funnel for brand-new requests; handoff message to network work queue */
int
mem_new_request(struct message *m)
{
    int err = -1;
    printd("new message\n");
    if (m->type == MSG_REQ_ALLOC && m->status == MSG_REQUEST)
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
