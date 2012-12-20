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

/* TODO need list representing pending alloc requests */

static struct queue work_q; /* work requests (messages) go here */

static struct message_forward nw_forward; /* to export msgs to network */
static struct message_forward mq_forward; /* to export msgs to MQ interface */

static pthread_t handler_tid;
/* volatile: gcc optimizes out updates to variable across threads */
static volatile bool handler_alive = false;

/* Private functions */

/* forward declaration */
static int export_msg_io(struct message *, int);
static int export_msg_mq(struct message *, pid_t);

static int
process_req_alloc(struct message *m)
{
    /* master node receiving new alloc requests from apps */
    if (m->status == MSG_REQUEST)
    {
        BUG(nw_get_rank() != 0);
        /* make request, copy result */
        struct alloc_ation a;
        printf("rank0 recv msg: alloc req\n");
        BUG(alloc_find(&m->u.req, &a) < 0);
        m->u.alloc = a;
        m->status = MSG_RESPONSE;
        export_msg_io(m, m->rank); /* return to origin */
    }
    /* TODO RESP: new msg DO_ALLOC, send to rank */
    else if (m->status == MSG_RESPONSE)
    {
        printf("rank%d recv msg: alloc resp, convert to do_alloc command\n",
                nw_get_rank());
        m->type = MSG_DO_ALLOC;
        m->status = MSG_REQUEST;
        /* XXX only send a message if the allocation required a remote
         * allocation. otherwise send messages back to the application to have
         * it perform local allocs, etc.
         *
         * library will need to reply with the result.. perhaps create a new
         * message type for that??
         */
        export_msg_io(m, m->u.alloc.rank);
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
        export_msg_io(m, m->rank); /* return to origin */
    }
    /* TODO RESP: insert to MQ to return to app */
    else if (m->status == MSG_RESPONSE)
    {
        printf("rank%d msg: do alloc resp, return to app via MQ\n",
                nw_get_rank());
        m->status = MSG_EOL;
        /*export_msg_io(m, m->pid);*/ /* return to app via MQ */
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

/* export message to network */
static int
export_msg_io(struct message *m, int rank)
{
    if (!nw_forward.handle)
        return -1;
    if (nw_forward.handle(nw_forward.q, m, rank) < 0) {
        printd("error forwarding msg to nw\n");
        return -1;
    }
    return 0;
}

/* export messages back to applications */
static int
export_msg_mq(struct message *m, pid_t pid)
{
    if (!mq_forward.handle)
        return -1;
    if (mq_forward.handle(mq_forward.q, m, pid) < 0) {
        printd("error forwarding msg to mq\n");
        return -1;
    }
    return 0;
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

/* message received from application */
int
mem_umsg_recv(struct message *m)
{
    int err = -1;
    printd("new message\n");
    /* XXX create a new request entry in a list.. each incoming message from
     * either app or nw will confirm portions of the allocation */
    if (m->type == MSG_REQ_ALLOC && m->status == MSG_REQUEST) {
        if (nw_forward.handle)
            err = nw_forward.handle(nw_forward.q, m, 0/*rank*/);
    } else {
        /* XXX msg might be a reponse to a message originally from us telling
         * app to do something */
    }
    return err;
}

int
mem_set_export(struct message_forward *f)
{
    if (!f) return -1;
    mq_forward.handle = f->handle;
    mq_forward.q = f->q;
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
