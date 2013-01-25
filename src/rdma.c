/* file: rdma.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: InfiniBand RDMA helper functions and threading code
 *
 * The list of allocs in this file will either be all server-side if the process
 * is a daemon, or all client-side if the process is the application (i.e. this
 * file is within the ocm library).
 */

/* System includes */
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

/* Project includes */
#include <util/list.h>
#include <io/rdma.h>
#include <debug.h>

/* Directory includes */
#include "rdma.h"

/* Globals */

/* Internal definitions */

/* Internal state */

static LIST_HEAD(ib_allocs);

/* Private functions */

/* only used by client code */
static int
post_send(struct ib_alloc *ib, int opcode, size_t len)
{
    struct ibv_sge          sge;
    struct ibv_send_wr      wr;
    struct ibv_send_wr      *bad_wr;

    sge.addr   = (uintptr_t) ib->params.buf; /* "from" address */
    sge.length = len;
    sge.lkey   = ib->verbs.mr->lkey; /* "from" key */

    memset(&wr, 0, sizeof(wr));

    wr.wr_id                = 1 /* ignored: user-defined ID */;
    wr.opcode               = opcode;
    /* This flag is needed so we can poll on send/recv using the Completion
     * Queue data structure. */
    wr.send_flags           = IBV_SEND_SIGNALED;
    wr.sg_list              = &sge;
    wr.num_sge              = 1;
    wr.wr.rdma.rkey         = ib->ibv.buf_rkey; /* "to" key */
    wr.wr.rdma.remote_addr  = ib->ibv.buf_va; /* "to" address */

    if (ibv_post_send(ib->rdma.id->qp, &wr, &bad_wr)){
        perror("ibv_post_send");
        return -1;
    }

    return 0;
}

/* Public functions */

int
ib_init(void)
{
    /* TODO in case we need to add init stuff later */
    return 0;
}

ib_t
ib_new(struct ib_params *p)
{
    struct ib_alloc *ib = NULL;

    if (!p)
        goto fail;

    ib = calloc(1, sizeof(*ib));
    if (!ib)
        goto fail;

    if (p->addr) /* only client specifies this */
        ib->params.addr = strdup(p->addr);
    memcpy(&ib->params, p, sizeof(*p));

    INIT_LIST_HEAD(&ib->link);
    list_add(&ib->link, &ib_allocs);

    return (ib_t)ib;

fail:
    return (ib_t)NULL;
}

/* TODO provide an accept and connect separately, instead of the bool */
int
ib_connect(ib_t ib, bool is_server)
{
    int err;

    if (!ib)
        return -1;

    if (is_server)
        err = ib_server_connect((struct ib_alloc*)ib);
    else
        err = ib_client_connect((struct ib_alloc*)ib);

    return err;
}

int
ib_reg_mr(ib_t ib, void *buf, size_t len)
{
    if (!ib || !buf || len == 0)
        return -1;

    printf(">> registering memory\n");

    /* overwrite existing values */
    ib->params.buf      = buf;
    ib->params.buf_len  = len;

    ib->verbs.mr = ibv_reg_mr(ib->verbs.pd, buf, len,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
            IBV_ACCESS_REMOTE_WRITE);

    if(!ib->verbs.mr)
        return -1; 

    return 0;
}

/* TODO include offset into buf */

/* client function: pull data fom server */
int
ib_read(ib_t ib, size_t len)
{
    if (!ib) return -1;
    return post_send(ib, IBV_WR_RDMA_READ, len);
}

/* client function: push data to server */
int
ib_write(ib_t ib, size_t len)
{
    if (!ib) return -1;
    return post_send(ib, IBV_WR_RDMA_WRITE, len);
}

/* Wait for some event. Code found in manpage of ibv_get_cq_event */
int
ib_poll(ib_t ib)
{
    struct ibv_wc   wc;
    struct ibv_cq   *evt_cq;
    void            *cq_ctxt;
    int             ne;

    if (ibv_req_notify_cq(ib->verbs.cq, 0))
        return -1;

    if (ibv_get_cq_event(ib->verbs.ch, &evt_cq, &cq_ctxt))
        return -1;

    ibv_ack_cq_events(evt_cq, 1);

    if (ibv_req_notify_cq(evt_cq, 0))
        return -1;

    do {
        ne = ibv_poll_cq(ib->verbs.cq, 1, &wc);

        if (ne == 0)
            continue;
        else if (ne < 0)
            return -1;

        if (wc.status != IBV_WC_SUCCESS)
            return -1;

    } while (ne);

    return 0;
}

/* TODO server functions */
/* right now the server is stupid, just helps make memory then goes away */
