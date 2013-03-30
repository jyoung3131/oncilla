/**
 * file: oncillamem.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: app library interface for OCM
 */

#ifndef __ONCILLAMEM_H__
#define __ONCILLAMEM_H__

/* System includes */
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/* Other project includes */

/* Project includes */
//#include <util/list.h>
#include <util/list.h>

/* Directory includes */

/* Definitions */

typedef struct lib_alloc * ocm_alloc_t;

enum ocm_kind
{
    OCM_LOCAL_HOST = 1,
    OCM_LOCAL_RMA,
    OCM_REMOTE_RMA,
    OCM_LOCAL_RDMA,
    OCM_REMOTE_RDMA,
    OCM_LOCAL_GPU,
    OCM_REMOTE_GPU,
};

struct ocm_params
{
    uint64_t src_offset;  
    uint64_t dest_offset;
    uint64_t bytes;
    //read = 0, write = 1
    int op_flag;
};

typedef struct ocm_params * ocm_param_t;


/* Globals */

/* Private functions */

/* Global functions */

int ocm_init(void);
int ocm_tini(void);
ocm_alloc_t ocm_alloc(size_t bytes, enum ocm_kind kind);
int ocm_free(ocm_alloc_t a);

/* get pointer to local buffer */
int ocm_localbuf(ocm_alloc_t a, void **buf, size_t *len);

bool ocm_is_remote(ocm_alloc_t a);

/* get size of remote buffer */
int ocm_remote_sz(ocm_alloc_t a, size_t *len);

int ocm_copy_out(void *dst, ocm_alloc_t src);
int ocm_copy_in(ocm_alloc_t dst, void *src);

int ocm_copy(ocm_alloc_t dst, ocm_alloc_t src, ocm_param_t options);

int ocm_copy_onesided(ocm_alloc_t src, ocm_param_t options); 
#endif  /* __ONCILLAMEM_H__ */
