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

static struct queue msg_q; /* incoming pending messages (from nw) */
static struct queue *outbox; /* msgs intended for apps  (to pmsg) */

static pthread_t handler_tid;
/* volatile: gcc optimizes out updates to variable across threads */
static volatile bool handler_alive = false;

/* Private functions */

static void
send_rank(struct message *m, int to_rank)
{
    printd("sending to rank %d via MPI\n", to_rank);
    nw_send(m, to_rank);
}

static void
send_pid(struct message *m, pid_t to_pid)
{
    printd("sending to app %d\n", to_pid);
    m->pid = to_pid;
    q_push(outbox, m);
}

/* each mpi rank inserts this message when it starts up */
static int
process_add_node(struct message *m)
{
    BUG(nw_get_rank() > 0);
    if (alloc_add_node(m->rank, &m->u.node.config))
        return -1;
    return 0;
}

/* message type MSG_REQ_ALLOC */
static int
process_req_alloc(struct message *m)
{
    int err;
    /* new allocation request */
    if (m->status == MSG_REQUEST) {
        BUG(nw_get_rank() > 0);
        struct alloc_ation alloc;
        printd("got msg from pid %d rank %d\n", m->pid, m->rank);
        /* make request, copy result */
        m->u.req.orig_rank = m->rank;
        err = alloc_find(&m->u.req, &alloc);
        BUG(err < 0);
        m->u.alloc = alloc; /* this assignment destroys req state */
        m->status = MSG_RESPONSE;
        /* TODO add new allocation to internal list as pending */
        send_rank(m, m->rank); /* return to origin */
    }
    else if (m->status == MSG_RESPONSE) {
        m->type = MSG_DO_ALLOC;
        m->status = MSG_REQUEST;
        if (m->u.alloc.type == ALLOC_MEM_HOST) {
            printd("send msg %d to pid %d\n", m->type, m->pid);
            send_pid(m, m->pid);
        } else if (m->u.alloc.type == ALLOC_MEM_RDMA) {
            /* rank will set up RDMA CM server, waiting for client to connect.
             * It will send a DO_ALLOC response, we'll then tell the app to
             * connect. */
            printd("send msg %d to rank %d\n", m->type, m->rank);
            send_rank(m, m->rank); 
        } else if (m->u.alloc.type == ALLOC_MEM_RMA) {
            printd("RMA allocations not yet coded\n");
            BUG(1);
        } else {
            BUG(1);
        }
    }
    return 0;
}

static void *
alloc_thread(void *arg)
{
    struct message *m = (struct message*)arg;
    BUG(!m);
    printd("thread spawned to wait for client connection\n");
    alloc_ate(&m->u.alloc); /* initialize RDMA CM server (blocks!) */
    printd("done\n");
    pthread_exit(NULL);
}

/* message type MSG_DO_ALLOC */
/* XXX we assume an allocation request is not partitioned for now. this means
 * that a response to a do_alloc is the last remaining message before releasing
 * the application */
static int
process_do_alloc(struct message *m)
{
    pthread_t pid;
    /* in-coming RMA/RDMA request from another node */
    if (m->status == MSG_REQUEST) {
        m->status = MSG_RESPONSE;
        send_rank(m, m->rank); /* tell app to connect to us */
        if (pthread_create(&pid, NULL, alloc_thread, (void*)m))
            ABORT();
    }
    /* in-coming response from app or rank that it completed a DO_ALLOC request */
    else if (m->status == MSG_RESPONSE) {
        m->type = MSG_RELEASE_APP;
        m->status = MSG_NO_STATUS;
        /* TODO put allocation state into message, send to app and rank 0 */
        send_pid(m, m->pid);
    }
    return 0;
}

static int
process_do_free(struct message *m)
{
    /* TODO REQ: call libRMA/etc, send reply (status to response) */
    if (m->status == MSG_REQUEST) {
    }
    /* TODO RESP: depends who sent request ...
     *              i) process explicitly requested free: to MQ
     *              ii) process died, drop this message
     */
    else if (m->status == MSG_RESPONSE) {
    }
    m->status++;
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

    while (true) {
        while (!q_empty(&msg_q)) {
            if (q_pop(&msg_q, &msg) != 0)
                BUG(1);

            printd("rank%d recv msg %s [%s]\n", nw_get_rank(),
                    MSG_TYPE2STR(msg.type), MSG_STATUS2STR(msg.status));

            err = 0;
            if (msg.type == MSG_REQ_ALLOC)
                err = process_req_alloc(&msg);
            else if (msg.type == MSG_DO_ALLOC)
                err = process_do_alloc(&msg);
            else if (msg.type == MSG_ADD_NODE)
                err = process_add_node(&msg);
            else {
                BUG(1);
            }

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
    printd("memory interface initializing\n");

    if (nw_init() < 0)
        return -1;

    q_init(&msg_q, sizeof(struct message));
    nw_set_recv_q(&msg_q);

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

void
mem_fin(void)
{
    nw_fin();

    if (!handler_alive) return;
    printd("memory interface finalizing\n");
    if (0 == pthread_cancel(handler_tid))
        pthread_join(handler_tid, NULL);
    while (handler_alive) ;
    q_free(&msg_q);
}

/* message received from application */
int
mem_add_msg(struct message *m)
{
    if (!m) return -1;
    if (nw_get_rank() > 0)
        send_rank(m, 0); /* send all messages from app directly to rank 0 */
    else
        q_push(&msg_q, m); /* we are rank0, no need to send to ourself */
    return 0;
}

void
mem_set_outbox(struct queue *ob)
{
    if (!ob)
        return;
    outbox = ob;
}
