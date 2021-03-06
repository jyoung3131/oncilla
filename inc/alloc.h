/**
 * file: alloc.h
 * authors: Alexander Merritt, merritt.alex@gatech.edu
 *          Jeff Young, jyoung9@gatech.edu
 * desc: interface for memory allocation governer
 */

#ifndef __ALLOC_H__
#define __ALLOC_H__

/* System includes */
#include <stdint.h>
#include <limits.h> /* HOST_NAME_MAX lives here */

/* Other project includes */

/* Project includes */
#include <util/list.h>
#ifdef INFINIBAND
  #include <io/rdma.h>
#endif

#ifdef EXTOLL
  #include <io/extoll.h>
#endif


/* Defines */

#define ALLOC_MAX_GPUS  8

enum alloc_ation_type
{
    ALLOC_MEM_INVALID = 0,

    ALLOC_MEM_HOST, /* local RAM only */
    ALLOC_MEM_RMA, /* EXTOLL */
    ALLOC_MEM_RDMA, /* infiniband */
    ALLOC_MEM_GPU, /* local or remote GPU */

    ALLOC_MEM_MAX
};

/* Types */

struct alloc_request
{
    int orig_rank;  /* originating rank where app is */
    int remote_rank; /* node requested for allocation, TODO not yet used */
    size_t bytes;
    enum alloc_ation_type type;
    /* TODO other properties */
};

///Structure that holds configuration information
///for each node; not currently required
struct alloc_node_config
{
    char ib_ip[HOST_NAME_MAX];
    size_t ram; /* host memory, bytes */
    size_t gpu_mem[ALLOC_MAX_GPUS]; /* same but for the GPUs */
    int num_gpu;
    /* TODO other stuffs */
};

struct alloc_ation
{
    struct list_head link;

    int orig_rank;
    int remote_rank;
    //A sequentially increasing ID used to find
    //and release remote allocations
    uint64_t rem_alloc_id;

    enum alloc_ation_type type;
    size_t bytes;

    union {
        #ifdef EXTOLL
        struct {
          RMA2_Nodeid node_id; //uint16_t
          RMA2_VPID vpid;    //uint16_t
          RMA2_NLA dest_nla; //uint64_t
          //Temp EXTOLL object used to help close server. Remove
          //once the free path is written
          extoll_t ex_rem;
        } rma;
        #endif
        #ifdef INFINIBAND
        struct {
            /* RDMA CM needs these */
            char ib_ip[HOST_NAME_MAX];
            int port;
            ib_t ib_rem;
        } rdma;
        #endif
    } u;
};

#if 0
enum alloc_do_type
{
    ALLOC_DO_INVALID = 0,
    ALLOC_DO_MALLOC,
    ALLOC_DO_RMA
};
#endif

/* Global state (externs) */

/* Static inline functions */

/* Function prototypes */

int alloc_add_node(int rank, struct alloc_node_config *config/*in*/);
int alloc_find(struct alloc_request *r/*in*/, struct alloc_ation *a/*out*/);
int alloc_ate(struct alloc_ation *a/*in*/);
int dealloc_ate(struct alloc_ation *a/*in*/);

#endif  /* __ALLOC_H__ */
