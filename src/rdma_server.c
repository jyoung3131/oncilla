/* file: rdma_server.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: File taken from Adit Ranadive's commlib RDMA code and refactored for
 * OCM
 */

/* System includes */
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Project includes */
#include <io/rdma.h>

/* Directory includes */
#include "rdma.h"

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */

int
ib_server_connect(struct ib_alloc *ib)
{
    /* 1. Set up RDMA CM structures */

    struct sockaddr_in addr;

    if (!(ib->rdma.ch = rdma_create_event_channel()))
        return -1;

    if (rdma_create_id(ib->rdma.ch, &ib->rdma.listen_id, NULL, RDMA_PS_TCP))
        return -1;

    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(ib->params.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind to local port and listen for
     * connection request */

    if (rdma_bind_addr(ib->rdma.listen_id, (struct sockaddr *) &addr))
        return -1;

    if (rdma_listen(ib->rdma.listen_id, 1))
        return -1;

    if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt)) /* blocks */
        return -1;

    if (ib->rdma.evt->event != RDMA_CM_EVENT_CONNECT_REQUEST)
        return -1;

    /* New CM identifier from client, used for subsequent reads/writes. */
    ib->rdma.id = ib->rdma.evt->id;

    rdma_ack_cm_event(ib->rdma.evt);

    /* 2. Create verbs objects now that we know which device to use */

    struct ibv_recv_wr recv_wr;

    memset(&recv_wr, 0, sizeof(recv_wr));

    if (!(ib->verbs.pd = ibv_alloc_pd(ib->rdma.id->verbs)))
        return -1; 

    if (!(ib->verbs.ch = ibv_create_comp_channel(ib->rdma.id->verbs)))
        return -1; 

    if (!(ib->verbs.cq =
                ibv_create_cq(ib->rdma.id->verbs, 100,
                    NULL, ib->verbs.ch, 0)))
        return -1; 

    if (ibv_req_notify_cq(ib->verbs.cq, 0)) 
        return -1; 

    uint32_t mr_flags =
        (IBV_ACCESS_LOCAL_WRITE |
         IBV_ACCESS_REMOTE_READ |
         IBV_ACCESS_REMOTE_WRITE);

    if (!(ib->verbs.mr = ibv_reg_mr(ib->verbs.pd, (void*)ib->params.buf,
                    ib->params.buf_len, mr_flags)))
        return -1;

    ib->verbs.qp_attr.cap.max_send_wr  = 2;
    ib->verbs.qp_attr.cap.max_send_sge = 2;
    ib->verbs.qp_attr.cap.max_recv_wr  = 2;
    ib->verbs.qp_attr.cap.max_recv_sge = 2;

    ib->verbs.qp_attr.send_cq = ib->verbs.cq;
    ib->verbs.qp_attr.recv_cq = ib->verbs.cq;

    ib->verbs.qp_attr.qp_type = IBV_QPT_RC;

    if (rdma_create_qp(ib->rdma.id, ib->verbs.pd, &ib->verbs.qp_attr))
        return -1;

    /* don't need to post a recv */
#if 0
    struct ibv_sge sge;
    sge.addr    = (uintptr_t) ib->params.buf;
    sge.length  = ib->params.buf_len;
    sge.lkey    = ib->verbs.mr->lkey;

    struct ibv_recv_wr wr, *bad_wr;
    memset(&wr, 0, sizeof(wr));
    wr.sg_list  = &sge;
    wr.num_sge  = 1;

    if (ibv_post_recv(ib->rdma.id->qp, &wr, &bad_wr))
        return 1;
#endif

    /* 3. Accept connection */

    /* this data is sent to client */
    struct __pdata_t pdata;
    pdata.buf_rkey = htonl(ib->verbs.mr->rkey);
    pdata.buf_va   = htonll((uintptr_t)ib->params.buf);

    ib->rdma.param.responder_resources  = 2;
    ib->rdma.param.private_data         = &pdata;
    ib->rdma.param.private_data_len     = sizeof(pdata);
    ib->rdma.param.initiator_depth      = 2;
    ib->rdma.param.retry_count          = 10;
    ib->rdma.param.rnr_retry_count      = 10;

    if (rdma_accept(ib->rdma.id, &ib->rdma.param))
        return -1;

    if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt))
        return -1;

    if (ib->rdma.evt->event != RDMA_CM_EVENT_ESTABLISHED)
        return -1;

    rdma_ack_cm_event(ib->rdma.evt);

    return 0;
}
