/**
 * file: rdma.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: interface for managing RDMA allocations and data movement
 */

#ifndef __RDMA_H__
#define __RDMA_H__

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Other project includes */

/* Project includes */

/* Defines */

/* Types */

struct ib_alloc; /* forward declaration */
typedef struct ib_alloc * ib_t;

struct ib_params {
    char        *addr; /* TODO used only by client */
    uint32_t    port;  /* TODO used only by client */
    void        *buf;
    size_t      buf_len;
};

/* Global state (externs) */

/* Function prototypes */

int ib_init(void);
ib_t ib_new(struct ib_params *p);
int ib_connect(ib_t ib, bool is_server);
int ib_reg_mr(ib_t ib, void *buf, size_t len);
int ib_read(ib_t ib, size_t len);
int ib_write(ib_t ib, size_t len);
int ib_poll(ib_t ib);

/* TODO include func to change remote mapping of local buf */

#endif  /* __RDMA_H__ */
