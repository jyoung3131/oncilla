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
#include <limits.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <alloc.h>
#include <debug.h>
#include <msg.h>
#include <sock.h>
#include <util/queue.h>
#include <nodefile.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

/* Internal state */
static int myrank = -1;

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

/* send then recv 1 message with rank */
static int
send_recv_msg(struct message *msg, int rank)
{
    int ret = -1, i = 2;
    char port[HOST_NAME_MAX];
    struct sockconn conn;
    memset(&conn, 0, sizeof(conn));
    BUG(!msg);
    BUG(rank > node_file_entries - 1);
    snprintf(port, HOST_NAME_MAX, "%d", node_file[rank].ocm_port);
    if (conn_connect(&conn, node_file[rank].ip_eth, port))
        goto out;
    while (i-- > 0) {
        switch (i) {
        case 1: ret = conn_put(&conn, msg, sizeof(*msg)); break;
        case 0: ret = conn_get(&conn, msg, sizeof(*msg)); break;
        }
        if (--ret < 0) /* 0 means remote closed; turn into error condition */
            break;
    }
    if (ret < 0 || (ret = conn_close(&conn)))
        goto out;
    ret = 0;
out:
    /* TODO close connection on error */
    return ret;
}

/* send 1 message to rank */
static int
send_msg(struct message *msg, int rank)
{
    int ret = -1;
    char port[HOST_NAME_MAX];
    struct sockconn conn;
    memset(&conn, 0, sizeof(conn));
    BUG(!msg);
    BUG(rank > node_file_entries - 1);
    snprintf(port, HOST_NAME_MAX, "%d", node_file[rank].ocm_port);
    if (conn_connect(&conn, node_file[rank].ip_eth, port))
        goto out;
    ret = conn_put(&conn, msg, sizeof(*msg));
    if (--ret < 0) /* 0 means remote closed; turn into error condition */
        goto out;
    if ((ret = conn_close(&conn)))
        goto out;
    ret = 0;
out:
    return ret;
}

/* message handlers */

static int
__msg_add_node(struct message *msg)
{
    return alloc_add_node(msg->rank, &msg->u.node.config);
}

static int
msg_send_add_node(struct message *msg)
{
    int ret;
    if (myrank == 0)
        ret = __msg_add_node(msg);
    else
        ret = send_msg(msg, 0);
    return ret;
}

static int
msg_recv_add_node(struct message *msg)
{
    BUG(!msg); BUG(myrank != 0);
    return __msg_add_node(msg);
}

static void
__msg_do_alloc(struct message *msg)
{
    BUG(alloc_ate(&msg->u.alloc));
}

static int
msg_send_do_alloc(struct message *msg)
{
    int ret = 0;
    BUG(!msg);
    BUG(msg->type != MSG_DO_ALLOC);
    BUG(msg->u.alloc.remote_rank > node_file_entries - 1);
    BUG(msg->u.alloc.remote_rank == myrank);

    ret = send_recv_msg(msg, msg->u.alloc.remote_rank);
    if (ret)
        goto out;
    send_pid(msg, msg->pid);

    ret = 0;
out:
    return ret;
}

static void
msg_recv_do_alloc(struct message *msg)
{
    BUG(!msg);
    __msg_do_alloc(msg);
}

/* TODO */
#if 0

static void
__msg_do_free(struct message *msg) { }

static int
msg_send_do_free(struct message *msg) { }

static void
msg_recv_do_free(struct message *msg) { }

#endif

static void
__msg_req_alloc(struct message *msg)
{
    struct alloc_ation alloc;
    msg->u.req.orig_rank = msg->rank;
    BUG(0 > alloc_find(&msg->u.req, &alloc));
    msg->u.alloc = alloc;
    msg->status++;
}

///Sends a request message to rank 0 to find a node for an allocation,
///then sends a subsquent message to rank N to request an allocation

static int
msg_send_req_alloc(struct message *msg)
{
    int ret = 0;
    BUG(!msg);
    BUG(msg->type != MSG_REQ_ALLOC);

    printd("requesting alloc from rank0\n");
    if (myrank == 0)
        __msg_req_alloc(msg);
    else
        ret = send_recv_msg(msg, 0);
    if (ret)
        goto out;

    printd("got alloc type %d\n", msg->u.alloc.type);
    if (msg->u.alloc.type != ALLOC_MEM_HOST) {
        msg->type   = MSG_DO_ALLOC;
        msg->status = MSG_REQUEST;
        /* TODO support multiple allocs across nodes here */
        ret = send_recv_msg(msg, msg->u.alloc.remote_rank);
        if (ret)
            goto out;
    }
    ret = 0;
out:
    return ret;
}

static void
msg_recv_req_alloc(struct message *msg)
{
    BUG(!msg); BUG(myrank != 0);
    printd("got msg from rank%d\n", msg->rank);
    __msg_req_alloc(msg);
}

/* threads */

/* <-- process requests from other daemons */
static void *
inbound_thread(void *arg)
{
    struct sockconn *conn = (struct sockconn*)arg;
    struct message msg;
    int ret = 0;
    BUG(!conn);
    printd("spawned\n");
    while (true) {
        ret = conn_get(conn, &msg, sizeof(msg));
        if (ret < 1)
            break;
        printd("got msg %s\n", MSG_TYPE2STR(msg.type));
        if (msg.type == MSG_ADD_NODE) {
            alloc_add_node(msg.rank, &msg.u.node.config);
        } else if (msg.type == MSG_REQ_ALLOC) {
            //Currently only rank 0 can handle inital allocation request
            //messages to determine the rank of the node that will fulfill
            //the allocation
            BUG(myrank != 0);
            msg_recv_req_alloc(&msg);
            ret = conn_put(conn, &msg, sizeof(msg));
            if (--ret < 0)
                break;
        } else if (msg.type == MSG_DO_ALLOC) {
            #ifdef INFINIBAND
            /* First, send msg back to orig rank to unblock app, so it can
             * initiate connection to us. Then listen for connections.
             * XXX possible race condition
             */
            ret = conn_put(conn, &msg, sizeof(msg));
            if (--ret < 0)
                break;
            msg_recv_do_alloc(&msg); /* blocks */
            #endif
            #ifdef EXTOLL
            /* EXTOLL server allocations are nonblocking and the call to
             * alloc_ate should return the needed setup parameters for the client
             * in msg.
             */
            msg_recv_do_alloc(&msg); /* should not block for EXTOLL setup */
            ret = conn_put(conn, &msg, sizeof(msg));
            #endif
        } else {
            printd("unhandled message %s\n", MSG_TYPE2STR(msg.type));
            BUG(1);
        }
    }
    printd("exiting %s\n", (ret < 0 ? "with error" : "normally"));
    if (ret) BUG(1);
    return NULL;
}


///listen_thread is spawned on each node from the mem_init call and it
///creates a connection to a socket on the OCM port. It also spawns
///inbound_thread to listen for new messages on this port
static void *
listen_thread(void *arg) /* persistent */
{
    struct sockconn conn;
    struct sockconn newconn, *newp;
    pthread_t tid; /* not used */
    int ret = -1;
    char port[HOST_NAME_MAX];

    snprintf(port, HOST_NAME_MAX, "%d", node_file[myrank].ocm_port);
    printd("listening on port %s\n", port);
    if (conn_localbind(&conn, port))
        goto out;

    while (true) {
        if ((ret = conn_accept(&conn, &newconn)))
            break;
        ret = -1;
        if (!(newp = malloc(sizeof(*newp))))
            break;
        *newp = newconn;
        if (pthread_create(&tid, NULL, inbound_thread, (void*)newp))
            break;
        if (pthread_detach(tid))
            break;
    }

out:
    __detailed_print("oops, I shouldn't be exiting!\n");
    BUG(1);
    return NULL;
}

/* local req --> send messages out and coordinate to fulfill request */
static void *
request_thread(void *arg)
{
    int ret = -1;
    struct message *msg = (struct message*)arg;
    BUG(!msg);
    printd("spawned, msg %s\n", MSG_TYPE2STR(msg->type));
    msg->rank = myrank;
    if (msg->type == MSG_ADD_NODE) {
        ret = msg_send_add_node(msg);
    } else if (msg->type == MSG_REQ_ALLOC) {
        ret = msg_send_req_alloc(msg);
        msg->type = MSG_RELEASE_APP;
        send_pid(msg, msg->pid);
    } else {
        __detailed_print("unhandled message %s\n", MSG_TYPE2STR(msg->type));
        BUG(1);
    }
    if (ret)
        __detailed_print("error sending message %s\n",
                MSG_TYPE2STR(msg->type));
    free(msg);
    printd("exiting %s\n", (ret < 0 ? "with error" : "normally"));
    if (ret) BUG(1);
    pthread_exit(NULL);
}

/* Public functions */

int
mem_init(const char *nodefile_path)
{
    pthread_t tid; /* not used */
    printd("memory interface initializing\n");

    if (parse_nodefile(nodefile_path, &myrank))
        return -1;
    printd("i am rank %d\n", myrank);
    BUG(myrank < 0);

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
    ; /* TODO kill listen thread */
}

/* message received from application */
int
mem_new_request(struct message *m)
{
    int ret = -1;
    struct message *new_msg = malloc(sizeof(*new_msg));
    pthread_t tid;
    if (!m || !new_msg)
        goto out;
    *new_msg = *m;
    if (pthread_create(&tid, NULL, request_thread, (void*)new_msg))
        goto out;
    if (pthread_detach(tid))
        goto out;
    ret = 0;
out:
    return ret;
}

void
mem_set_outbox(struct queue *q)
{
    BUG(!(outbox = q));
}
