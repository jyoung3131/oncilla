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
#include <alloc.h>
#include <debug.h>
#include <msg.h>
#include <sock.h>
#include <util/queue.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

/* Internal state */

/* TODO need list representing pending alloc requests */

static struct queue *outbox; /* msgs intended for apps  (to pmsg) */

/* Private functions */

static void
send_pid(struct message *m, pid_t to_pid)
{
    printd("%s %d\n", __func__, to_pid);
    m->pid = to_pid;
    q_push(outbox, m);
}

#if 0
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

/* XXX refactor this somehow.. ugly use of a thread? */
static void *
alloc_thread(void *arg)
{
    struct message *m = (struct message*)arg;
    BUG(!m);
    printd("thread spawned to wait for client connection\n");
    alloc_ate(&m->u.alloc); /* initialize RDMA CM server (blocks!) */
    printd("done\n");
    free(m);
    m = NULL;
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
    struct message *mptr = malloc(sizeof(*mptr));
    ABORT2(!mptr);
    *mptr = *m;
    /* in-coming RMA/RDMA request from another node */
    if (m->status == MSG_REQUEST) {
        m->status = MSG_RESPONSE;
        send_rank(m, m->rank); /* tell app to connect to us */
        if (pthread_create(&pid, NULL, alloc_thread, (void*)mptr))
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
#endif

static void *
inbound_thread(void *arg)
{
    struct sockconn *conn = (struct sockconn*)arg;
    struct message msg;
    int ret = 0;

    BUG(!conn);

    while (true) {
        ret = conn_get(conn, &msg, sizeof(msg));
        if (ret < 1)
            break;
        if (msg.type == MSG_ADD_NODE)
            alloc_add_node(&msg.u.node.config);
        else
            ABORT();
    }

    if (ret < 0)
        printd("exiting with error\n");

    return NULL;
}

/* this thread is persistent */
static void *
listen_thread(void *arg)
{
    struct sockconn conn;
    struct sockconn newconn, *newp;
    pthread_t tid; /* not used */
    int ret = -1;

    printd("listening on port %d\n", 67890);
    if (conn_localbind(&conn, "67890"))
        goto exit;

    while (true) {
        if ((ret = conn_accept(&conn, &newconn)))
            break;
        ret = -1;
        newp = malloc(sizeof(*newp));
        if (!newp)
            break;
        if (pthread_create(&tid, NULL, inbound_thread, (void*)newp))
            break;
        if (pthread_detach(tid))
            break;
    }

exit:
    if (ret < 0)
        printd("exiting with error\n");

    return NULL;
}

static void *
request_thread(void *arg)
{
    //struct message *m = (struct message*)arg;
    pthread_exit(NULL);
}

/* Public functions */

int
mem_init(void)
{
    pthread_t tid; /* not used */
    printd("memory interface initializing\n");

    if (pthread_create(&tid, NULL, listen_thread, NULL))
        return -1;
    if (pthread_detach(tid))
        return -1;

    /* TODO collect memory information about node */

    return 0;
}

void
mem_fin(void)
{
    ; /* nothing */
}

/* message received from application */
int
mem_new_request(struct message *m)
{
    pthread_t tid;
    if (!m) return -1;
    if (pthread_create(&tid, NULL, request_thread, (void*)m))
        return -1;
    if (pthread_detach(tid))
        return -1;
    return 0;
}

void
mem_set_outbox(struct queue *q)
{
    BUG(!(outbox = q));
}
