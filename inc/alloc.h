/**
 * file: alloc.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
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
#include <io/rdma.h>

/* Defines */

#define ALLOC_MAX_GPUS  8

enum alloc_ation_type
{
    ALLOC_MEM_INVALID = 0,

    ALLOC_MEM_HOST, /* local RAM only */
    ALLOC_MEM_RMA, /* HTX */
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

struct alloc_node_config
{
    char hostname[HOST_NAME_MAX];
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

    enum alloc_ation_type type;
    size_t bytes;

    union {
        struct {
            /* TODO */
        } rma;
        struct {
            /* RDMA CM needs these */
            char ib_ip[HOST_NAME_MAX];
            int port;
        } rdma;
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

int alloc_add_node(int rank, struct alloc_node_config *c/*in*/);
int alloc_find(struct alloc_request *r/*in*/, struct alloc_ation *a/*out*/);
int alloc_ate(struct alloc_ation *a/*in*/);
int dealloc_ate(struct alloc_ation *a/*in*/);

#endif  /* __ALLOC_H__ */
