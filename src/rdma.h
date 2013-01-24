/* file: rdma.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: internal data structures for the RDMA interface
 *
 * Original code modified from Adit Ranadive's commlib sources.
 */

#ifndef __RDMA_INTERNAL_H__
#define __RDMA_INTERNAL_H__

#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <util/list.h>

enum {
    RESPONSE_TIME_MS = 5000
};

enum {
    RESOLVE_TIMEOUT_MS = 5000
};

struct __pdata_t {
    uint64_t    buf_va;
    uint64_t    buf_rkey;
};

struct __rdma_t {
    struct rdma_event_channel   *ch;
    struct rdma_cm_id           *listen_id;
    struct rdma_cm_id           *id;
    struct rdma_cm_event        *evt;
    struct rdma_conn_param      param;
};

struct __ibv_t {
    /* XXX Why do I need these three items? key values can be found in
     * ib->verbs.mr
     */
    unsigned long long  buf_va;
    unsigned            buf_rkey;
    //unsigned            buf_lkey;
    unsigned int        lid;
    unsigned int        qpn;
    unsigned int        psn;
};

struct __verbs_t {
    struct ibv_pd           *pd;
    struct ibv_comp_channel *ch;
    struct ibv_cq           *cq;
    struct ibv_cq           *evt_cq;
    struct ibv_mr           *mr;
    struct ibv_qp           *qp;
    struct ibv_qp_init_attr qp_attr;
    struct ibv_context      *context;
    //struct __ibv_t          keys;
};

struct ib_alloc
{
    struct list_head    link;
    struct __rdma_t     rdma;
    struct __ibv_t      ibv;
    struct __verbs_t    verbs;
    struct ib_params    params;
};

/* server functions */
int ib_server_connect(struct ib_alloc *ib);

/* client functions */
int ib_client_connect(struct ib_alloc *ib);

#endif
