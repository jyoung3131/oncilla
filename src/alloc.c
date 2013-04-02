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
#include <nodefile.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

/* Internal state */

/* BOTH lists maintained ONLY by rank 0 */

/* struct alloc_ation list */
static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;
static LIST_HEAD(allocs);
static unsigned int num_allocs = 0;

#define for_each_alloc(alloc, allocs) \
    list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()    pthread_mutex_lock(&allocs_lock)
#define unlock_allocs()  pthread_mutex_unlock(&allocs_lock)

/* Private functions */

/* Public functions */

int
alloc_add_node(int rank, struct alloc_node_config *config)
{
    struct node_entry *node;
    if (!config) return -1;
    BUG(rank > node_file_entries - 1);
    node = &node_file[rank];
    BUG(node->config);
    node->config = malloc(sizeof(*config));
    if (!node->config)
        return -1;
    *(node->config) = *config;
    printd("node joined: rank %d dns %s ib_ip %s\n",
            rank, node->dns, node->config->ib_ip);
    return 0;
}

int
alloc_find(struct alloc_request *req, struct alloc_ation *alloc)
{
    struct node_entry *node = NULL;

    if (!req || !alloc) return -1;

    if (node_file_entries == 1)
        req->type = ALLOC_MEM_HOST;

    alloc->orig_rank    = req->orig_rank;
    alloc->type         = req->type;
    alloc->bytes        = req->bytes; /* TODO validate size will fit on node */

    if (req->type == ALLOC_MEM_HOST)
        alloc->remote_rank = req->orig_rank;

    #ifdef INFINIBAND
    else if (req->type == ALLOC_MEM_RDMA) {
        printd("req orig rank %d, num nodes %d\n",
                req->orig_rank, node_file_entries);
        alloc->remote_rank = (req->orig_rank + 1) % node_file_entries; /* XXX */
        node = &node_file[alloc->remote_rank];
        strncpy(alloc->u.rdma.ib_ip, node->config->ib_ip, HOST_NAME_MAX);
        alloc->u.rdma.port = node->rdmacm_port;
        printd("alloc: rdma on %s rank %d\n",
                alloc->u.rdma.ib_ip, alloc->remote_rank);
    }
    #endif
    
    #ifdef EXTOLL
    else if (req->type == ALLOC_MEM_RMA) {
        node = &node_file[alloc->remote_rank];
        BUG(1); /* TODO */
    }
    #endif

    #ifdef CUDA
    else if (req->type == ALLOC_MEM_GPU)
        alloc->remote_rank = req->orig_rank;
    #endif

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

    if (!alloc)
        return -1;

    #ifdef INFINIBAND
    /* XXX Save this value somewhere. Maybe create unique alloc IDs to associate
     * with this. */
    ib_t ib;

    if (alloc->type == ALLOC_MEM_RDMA) {
        struct ib_params p;
        p.addr      = NULL;
        p.port      = alloc->u.rdma.port;
        p.buf_len   = alloc->bytes;
        p.buf       = calloc(1, alloc->bytes);
        ABORT2(!p.buf);
        if (!(ib = ib_new(&p)))
            ABORT();
        printd("RDMA: wait for client on port %d\n", p.port);
        if (ib_connect(ib, true))
            ABORT();
    }
    #endif

    #ifdef EXTOLL
    else if (alloc->type == ALLOC_MEM_RMA) {
        BUG(1);
    }
    #endif
    #ifdef CUDA
    else if (alloc->type == ALLOC_MEM_GPU){
    }
    #endif
    else {
        BUG(1);
    }

    return 0;
}
