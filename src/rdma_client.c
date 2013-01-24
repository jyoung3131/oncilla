/* file: rdma_client.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: File taken from Adit Ranadive's commlib RDMA code and refactored for
 * OCM
 */

/* System includes */
#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>

#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* Project includes */
#include <io/rdma.h>

/* Directory includes */
#include "rdma.h"

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */

int
ib_client_connect(struct ib_alloc *ib)
{
    int err = 0, n;
    struct addrinfo *res, *t, hints;
    char *service;

    memset(&hints, 0, sizeof(&hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    /* 1. Set up RDMA CM structures */

    if (!(ib->rdma.ch = rdma_create_event_channel()))
        return -1;

    if (rdma_create_id(ib->rdma.ch, &ib->rdma.id, NULL, RDMA_PS_TCP))
        return -1;

    if (0 > asprintf(&service, "%d", ib->params.port))
        return -1;

    n = getaddrinfo(ib->params.addr, service, &hints, &res);
    if (n < 0)
        return -1;

    /* resolve the address */
    err = 0;
    for (t = res; t; t = t->ai_next)
        if (!(err = rdma_resolve_addr(ib->rdma.id, NULL,
                    t->ai_addr, RESOLVE_TIMEOUT_MS)))
            break;
    if (err)
        return -1;

    /* pull and ack event */
    if (rdma_get_cm_event(ib->rdma.ch, &(ib->rdma.evt)))
        return -1;
    if (ib->rdma.evt->event != RDMA_CM_EVENT_ADDR_RESOLVED)
        return -1;
    rdma_ack_cm_event(ib->rdma.evt);

    /* resolve the route */
    if (rdma_resolve_route(ib->rdma.id, RESOLVE_TIMEOUT_MS))
        return -1;

    /* pull and ack event */
    if (rdma_get_cm_event(ib->rdma.ch, &(ib->rdma.evt)))
        return -1;
    if (ib->rdma.evt->event != RDMA_CM_EVENT_ROUTE_RESOLVED)
        return -1;
    rdma_ack_cm_event(ib->rdma.evt);

    /* 2. Create verbs objects now that we know which device to use */

    if (!(ib->verbs.pd = ibv_alloc_pd(ib->rdma.id->verbs)))
        return -1;

    if (!(ib->verbs.ch = ibv_create_comp_channel(ib->rdma.id->verbs)))
        return -1;

    if (!(ib->verbs.cq = ibv_create_cq(ib->rdma.id->verbs,
                    2, NULL, ib->verbs.ch, 0)))
        return -1;

    if (ibv_req_notify_cq(ib->verbs.cq, 0))
        return -1;

    uint32_t mr_flags =
        (IBV_ACCESS_LOCAL_WRITE |
         IBV_ACCESS_REMOTE_READ |
         IBV_ACCESS_REMOTE_WRITE);

    if (!(ib->verbs.mr = ibv_reg_mr(ib->verbs.pd, ib->params.buf,
                    ib->params.buf_len, mr_flags)))
        return -1;

    ib->verbs.qp_attr.cap.max_send_wr   = 2;
    ib->verbs.qp_attr.cap.max_send_sge  = 2;
    ib->verbs.qp_attr.cap.max_recv_wr   = 2;
    ib->verbs.qp_attr.cap.max_recv_sge  = 2;

    ib->verbs.qp_attr.send_cq   = ib->verbs.cq;
    ib->verbs.qp_attr.recv_cq   = ib->verbs.cq;

    ib->verbs.qp_attr.qp_type   = IBV_QPT_RC;

    if (rdma_create_qp(ib->rdma.id, ib->verbs.pd, &ib->verbs.qp_attr))
        return -1;

    /* 3. Connect to server */

    //ib->rdma.param.responder_resources  = 2;
    ib->rdma.param.initiator_depth      = 2;
    ib->rdma.param.retry_count          = 10;
    //ib->rdma.param.rnr_retry_count      = 10;

    if (rdma_connect(ib->rdma.id, &ib->rdma.param))
        return -1;

    if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt))
        return -1;

    if (ib->rdma.evt->event != RDMA_CM_EVENT_ESTABLISHED)
        return -1;

    struct __pdata_t pdata;
    memcpy(&pdata, ib->rdma.evt->param.conn.private_data, sizeof(pdata));
    ib->ibv.buf_rkey   = ntohl(pdata.buf_rkey);
    ib->ibv.buf_va     = ntohll(pdata.buf_va);

    rdma_ack_cm_event(ib->rdma.evt);

    return 0;
}
