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
#include <time.h>

/* Project includes */
#include <io/rdma.h>
#include <debug.h>

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

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  /* 1. Set up RDMA CM structures */

  if (!(ib->rdma.ch = rdma_create_event_channel()))
    return -1;

  if (rdma_create_id(ib->rdma.ch, &ib->rdma.id, NULL, RDMA_PS_TCP))
    return -1;

  printf("Port number in client_connect is %d\n", ib->params.port);
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
          ib->params.buf_len, mr_flags))) {
    perror("RDMA memory registration");
    return -1;
  }

  printd("registered memory region (%lu bytes)\n",
      ib->verbs.mr->length);

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

  printd("Connecting to server with rdma_connect\n");

  //resume total connection timer
  if (rdma_connect(ib->rdma.id, &ib->rdma.param))
    return -1;

  if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt))
    return -1;

  printd("Checking with server to make sure connection establisted\n");
  if (ib->rdma.evt->event != RDMA_CM_EVENT_ESTABLISHED)
  {
    printf("ib_client_connect:: RDMA event returned error code %d\n", ib->rdma.evt->event);
    return -1;
  }

  struct __pdata_t pdata;
  memcpy(&pdata, ib->rdma.evt->param.conn.private_data, sizeof(pdata));
  ib->ibv.buf_rkey    = ntohl(pdata.buf_rkey);
  ib->ibv.buf_va      = ntohll(pdata.buf_va);
  ib->ibv.buf_len     = ntohll(pdata.buf_len);
  printd("extracted rkey %u va 0x%llu len %llu\n",
      ib->ibv.buf_rkey, ib->ibv.buf_va, ib->ibv.buf_len);

  rdma_ack_cm_event(ib->rdma.evt);

  return 0;
}

  int
ib_client_disconnect(struct ib_alloc *ib)
{
  ////////////////////
  //IB Verbs events
  //////////////////////
  //-rdma_destroy_qp (use this instead of ibv_destroy_qp since we created the qp with rdma_create_qp)
  //-ibv_dereg_mr
  //-Can free the buffer at this point
  //-ibv_destroy_cq
  //-ibv_destroy_comp_channel
  //-ibv_dealloc_pd
  ////If we were using ibv device directly we would also need to call ibv_close_device
  //
  //////////////////////
  //Connection Manager events
  //////////////////////
  //-rdma_destroy_id
  //-rdma_destroy_event_channel
  //

  int rc = 0;

  if (rdma_disconnect(ib->rdma.id))
  {
    fprintf(stderr, "failed to disconnect RDMA connection\n");
    rc = 1;
  }


  //Destroy the queue pair - returns void
  rdma_destroy_qp(ib->rdma.id);


  //------deregister pinned pages---------
  if (ibv_dereg_mr(ib->verbs.mr))
  {
    fprintf(stderr, "failed to deregister MR\n");
    rc = 1;
  }

  //Make sure to free the buffer, ib->ib_params.buf in the dealloc function
  //free(res->buf);


  if (ibv_destroy_cq(ib->verbs.cq))
  {
    fprintf(stderr, "failed to destroy CQ\n");
    rc = 1;
  }

  if (ibv_destroy_comp_channel(ib->verbs.ch))
  {
    fprintf(stderr, "failed to destroy CQ channel\n");
    rc = 1;
  }

  if (ibv_dealloc_pd(ib->verbs.pd))
  {
    fprintf(stderr, "failed to deallocate PD\n");
    rc = 1;
  }

  rdma_destroy_id(ib->rdma.id);

  rdma_destroy_event_channel(ib->rdma.ch);

  //  printf("Successfully destroyed all IB and RDMA CM objects\n");

  return rc;

}

