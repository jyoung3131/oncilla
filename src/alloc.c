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
#include <sys/sysinfo.h>

/* Project includes */
#include <alloc.h>
#include <debug.h>
#include <util/list.h>
#include <util/mem.h>
#include <nodefile.h>

/* Directory includes */
#ifdef EXTOLL
#include "extoll.h"
#endif

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
    if (!req || !alloc) return -1;

    if (node_file_entries == 1)
        req->type = ALLOC_MEM_HOST;

    alloc->orig_rank    = req->orig_rank;
    alloc->type         = req->type;
    //Check to make sure the request is not greater than free memory
    /*if(req->bytes >= get_free_mem())
      BUG(1);
    */

    alloc->bytes        = req->bytes; /* TODO validate size will fit on node */

    if ((req->type == ALLOC_MEM_HOST) || (req->type == ALLOC_MEM_GPU))
    {
        alloc->remote_rank = req->orig_rank;
        printd("Host or local GPU: req orig rank %d, alloc rank %d\n",req->orig_rank, alloc->remote_rank);
    }

    #ifdef INFINIBAND
    else if (req->type == ALLOC_MEM_RDMA) {
        //defined locally to avoid unused variable error
        struct node_entry *node = NULL;

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
        printd("req orig rank %d, num nodes %d\n",
        req->orig_rank, node_file_entries);
                 alloc->remote_rank = (req->orig_rank + 1) % node_file_entries; /* XXX */
        printd("alloc: rma on rank %d\n", alloc->remote_rank);
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
    //Create a new allocation structure that can be saved on this node
    //to handle teardown
    struct alloc_ation *rem_alloc;
    rem_alloc = calloc(1, sizeof(*rem_alloc));

    if (!alloc)
        return -1;

    //Copy the remote allocation ID to the local struct
    rem_alloc->rem_alloc_id = alloc->rem_alloc_id;

    #ifdef INFINIBAND
    if (alloc->type == ALLOC_MEM_RDMA) {
        struct ib_params p;
        p.addr      = NULL;
        p.port      = alloc->u.rdma.port;
        p.buf_len   = alloc->bytes;
        p.buf       = calloc(1, alloc->bytes);
        ABORT2(!p.buf);
        if (!(rem_alloc->u.rdma.ib_rem = ib_new(&p)))
            ABORT();
        printd("RDMA: wait for client on port %d\n", p.port);
        if (ib_connect(rem_alloc->u.rdma.ib_rem, true))
            ABORT();
        
        rem_alloc->type = ALLOC_MEM_RDMA;
    }
    #endif

    #ifdef EXTOLL
    if (alloc->type == ALLOC_MEM_RMA) {
        struct extoll_params p;
        p.buf_len   = alloc->bytes;
        //We don't need to allocate the buffer since connect does this
        //for us
        ABORT2(!p.buf);
        if (!(rem_alloc->u.rma.ex_rem = extoll_new(&p)))
            ABORT();
        printd("EXTOLL: setting up server connection\n");
        if (extoll_connect(rem_alloc->u.rma.ex_rem, true))
            ABORT();

        printd("EXTOLL parameters for the server are NodeID: %d VPID: %d and NLA %lx\n", rem_alloc->u.rma.ex_rem->params.dest_node, rem_alloc->u.rma.ex_rem->params.dest_vpid, rem_alloc->u.rma.ex_rem->params.dest_nla);
        //Save these parameters into the parameter allocation struct so they get passed back in the return message to the client
        alloc->type = ALLOC_MEM_RMA;
        alloc->u.rma.node_id = rem_alloc->u.rma.ex_rem->params.dest_node;
        alloc->u.rma.vpid = rem_alloc->u.rma.ex_rem->params.dest_vpid;
        alloc->u.rma.dest_nla = rem_alloc->u.rma.ex_rem->params.dest_nla;
   
    }
    #endif
    #ifdef CUDA
    else if (alloc->type == ALLOC_MEM_GPU)
    {
      printf("Remote CUDA allocations not supported!\n");
    }
    #endif
    else {
        BUG(1);
    }

    //Add the local allocation ptr to a linked list so we can close the connection later
    INIT_LIST_HEAD(&alloc->link);
    printd("Adding new remote alloc with ID %lu to list\n", rem_alloc->rem_alloc_id);
    lock_allocs();
    list_add(&rem_alloc->link, &allocs);
    unlock_allocs();

    return 0;
}

/* This function deallocates any remote allocations for connections via IB or
 * EXTOLL. 
 *
 * This function is executed by any node which is present in a remote
 * allocation, returned by rank 0.
 */
int
dealloc_ate(struct alloc_ation *alloc)
{
    if (!alloc)
        return -1;
    
    //Pointer used to iterate over list of local allocation structs
    struct alloc_ation *tmp, *rem_alloc;

    rem_alloc=NULL;

    //Iterate over the list of remote allocations
    for_each_alloc(tmp, allocs)
    {
      if(tmp->rem_alloc_id == alloc->rem_alloc_id)
      {
        rem_alloc = tmp;
        break;
      }
    }

    if(rem_alloc == NULL)
    {
      printd("No remote allocation found for ID %lu \n", alloc->rem_alloc_id);
      BUG(1);
    }
    
    printd("Deallocating memory for allocation %lu of type %d\n", rem_alloc->rem_alloc_id, rem_alloc->type);

    #ifdef INFINIBAND
    if (alloc->type == ALLOC_MEM_RDMA) 
    {
        if (ib_disconnect(rem_alloc->u.rdma.ib_rem, true))
            ABORT();
    }
    #endif
    #ifdef EXTOLL
    if (alloc->type == ALLOC_MEM_RMA) 
    {

        if (extoll_disconnect(rem_alloc->u.rma.ex_rem, true))
            ABORT();
    }
    #endif 
    #ifdef CUDA
    if (alloc->type == ALLOC_MEM_GPU)
    {
      printf("Remote CUDA allocations not supported!\n");
    }
    #endif

    return 0;

}
