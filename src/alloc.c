/**
 * file: alloc.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: memory allocation governer
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
#include <util/list.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

struct node
{
    struct list_head link;
    int rank;
    struct alloc_node_config c;
};

/* Internal state */

/* BOTH lists maintained ONLY by rank 0 */

/* struct alloc_ation list */
static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(allocs);
static unsigned int num_allocs = 0;

/* struct node list */
static pthread_mutex_t nodes_lock = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(nodes);
static unsigned int num_nodes = 0;

#define for_each_node(node, nodes) \
    list_for_each_entry(node, &nodes, link)
#define lock_nodes()    pthread_mutex_lock(&nodes_lock)
#define unlock_nodes()  pthread_mutex_unlock(&nodes_lock)

#define for_each_alloc(alloc, allocs) \
    list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()    pthread_mutex_lock(&allocs_lock)
#define unlock_allocs()  pthread_mutex_unlock(&allocs_lock)

/* Private functions */

static struct node *
__find_node(int rank)
{
    struct node *n = NULL;
    for_each_node(n, nodes)
        if (n->rank == rank)
            break;
    return n;
}

/* Public functions */

int
alloc_add_node(int rank, struct alloc_node_config *c)
{
    struct node *n;

    if (!c) return -1;

    n = calloc(1, sizeof(*n));
    if (!n) ABORT();

    INIT_LIST_HEAD(&n->link);
    n->rank = rank;
    n->c = *c;

    lock_nodes();
    list_add(&n->link, &nodes);
    num_nodes++;
    unlock_nodes();

    /* TODO update this debug print to include all config info */
    printd("node joined: rank %d dns %s ib_ip %s\n",
            n->rank, n->c.hostname, n->c.ib_ip);
    return 0;
}

/* TODO remove node? MPI v3 is what supports dynamic process joining/removing
 * the communication set... otherwise we assume all ranks/nodes that are up are
 * all immediately available and don't go away */

/* consults the metadata to locate (and reserve) memories. only rank 0 executes
 * this code */
int
alloc_find(struct alloc_request *req, struct alloc_ation *alloc)
{
    struct node *node = NULL;

    if (!req || !alloc) return -1;

    if (num_nodes == 1)
        req->type = ALLOC_MEM_HOST;

    alloc->orig_rank    = req->orig_rank;
    alloc->type         = req->type;
    alloc->bytes        = req->bytes; /* TODO validate size will fit on node */

    if (req->type == ALLOC_MEM_HOST)
        alloc->remote_rank = req->orig_rank;
    
    else if (req->type == ALLOC_MEM_RDMA) {
        alloc->remote_rank = (req->orig_rank + 1) % num_nodes; /* XXX */
        BUG(!(node = __find_node(alloc->remote_rank)));
        strncpy(alloc->u.rdma.ib_ip, node->c.ib_ip, HOST_NAME_MAX);
        alloc->u.rdma.port = 12345; /* XXX pick a random number */
        printd("alloc: rdma on %s rank %d\n",
                alloc->u.rdma.ib_ip, alloc->remote_rank);
    }
    
    else if (req->type == ALLOC_MEM_RMA) {
        BUG(1); /* TODO */
    }

    else BUG(1);

    INIT_LIST_HEAD(&alloc->link);
    lock_allocs();
    list_add(&alloc->link, &allocs);
    num_allocs++;
    unlock_allocs();

    return 0;
}

/* This function should only carry out requests for allocation that necessitate
 * involvement of a remote node. Local allocations must be passed back to the
 * application for the library to make, so here we only allocate and register
 * remote memory.
 *
 * This function is executed by any node which is present in a remote
 * allocation, returned by rank 0.
 */
int
alloc_ate(struct alloc_ation *alloc)
{
    /* XXX Save this value somewhere. Maybe create unique alloc IDs to associate
     * with this. */
    ib_t ib;

    if (!alloc)
        return -1;

    if (alloc->type == ALLOC_MEM_RDMA) {
        struct ib_params p;
        p.addr      = NULL;
        p.port      = alloc->u.rdma.port;
        p.buf_len   = alloc->bytes;
        p.buf       = calloc(1, alloc->bytes);
        ABORT2(!p.buf);
        if (!(ib = ib_new(&p)))
            ABORT();
        if (ib_connect(ib, true))
            ABORT();
    }

    else if (alloc->type == ALLOC_MEM_RMA) {
        BUG(1);
    }

    else {
        BUG(1);
    }

    return 0;
}
