/**
 * file: alloc.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: interface for memory allocation governer
 */

#ifndef __ALLOC_H__
#define __ALLOC_H__

/* System includes */
#include <stdint.h>

/* Other project includes */

/* Project includes */
#include <util/list.h>

/* Defines */

#define ALLOC_MAX_GPUS  8

enum alloc_ation_type
{
    ALLOC_MEM_INVALID = 0,

    ALLOC_MEM_HOST,
    ALLOC_MEM_RMA,
    ALLOC_MEM_GPU,

    ALLOC_MEM_MAX
};

/* Types */

struct alloc_request
{
    int orig_rank; /* location request originates from */
    size_t bytes;
    /* TODO other properties */
};

struct alloc_node_config
{
    size_t ram; /* host memory, bytes */
    size_t gpu_mem[ALLOC_MAX_GPUS]; /* same but for the GPUs */
    int num_gpu;
    /* TODO other stuffs */
};

struct alloc_ation
{
    struct list_head link;
    /* resource-specific memory object identifiers */
    /* TODO make all these into arrays to handle partitioned memories */

    enum alloc_ation_type type;
    size_t bytes;
    /* TODO NLA, host pointers, GPU pointers, etc */
    uintptr_t ptr;
    int rank; /* node to provide the memory */

};

enum alloc_do_type
{
    ALLOC_DO_INVALID = 0,
    ALLOC_DO_MALLOC,
    ALLOC_DO_RMA
};

struct alloc_do
{
    enum alloc_do_type type;
    size_t bytes;
    uintptr_t ptr;
};

/* Global state (externs) */

/* Static inline functions */

/* Function prototypes */

int alloc_add_node(int rank, struct alloc_node_config *c/*in*/);
int alloc_find(struct alloc_request *r/*in*/, struct alloc_ation *a/*out*/);
int alloc_ate(struct alloc_ation *a/*in*/);

#endif  /* __ALLOC_H__ */
