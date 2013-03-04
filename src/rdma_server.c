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
#include <errno.h>
#include <time.h>

/* Project includes */
#include <io/rdma.h>
#include <util/timer.h>
#include <debug.h>

/* Directory includes */
#include "rdma.h"

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
ib_server_connect(struct ib_alloc *ib)
{
    /* 1. Set up RDMA CM structures */

    struct sockaddr_in addr;

    if (!(ib->rdma.ch = rdma_create_event_channel()))
        return -1;

    if (rdma_create_id(ib->rdma.ch, &ib->rdma.listen_id, NULL, RDMA_PS_TCP))
        return -1;

    printf("Port number in server_connect is %d\n", ib->params.port);

    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(ib->params.port);
    //addr.sin_port        = 12345;
    //inet_aton("10.0.0.2", (struct in_addr *)&addr.sin_addr.s_addr);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind to local port and listen for
     * connection request */
    printf("addr.sin_port = %d\n", addr.sin_port);

    if (rdma_bind_addr(ib->rdma.listen_id, (struct sockaddr *) &addr))
    {
        printf("addr.sin_port = %d\n", addr.sin_port);
        printf("IP address is %s\n",inet_ntoa(addr.sin_addr));    
        printf("rdma_bind_addr failed with errno %d\n", errno);
        return -1;
    }

    if (rdma_listen(ib->rdma.listen_id, 1))
        return -1;

    printd("waiting for connection on port %d...\n", ib->params.port);
    if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt)) /* blocks */
        return -1;

    printd("got connection\n");
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

  #ifdef TIMING    
  uint64_t ib_mem_reg_ns = 0;
  #endif
  TIMER_DECLARE1(ib_server_timer);
  TIMER_START(ib_server_timer);

    if (!(ib->verbs.mr = ibv_reg_mr(ib->verbs.pd, (void*)ib->params.buf,
                    ib->params.buf_len, mr_flags))) {
        perror("RDMA memory registration");
        return -1;
    }
  
    TIMER_END(ib_server_timer, ib_mem_reg_ns);

#ifdef TIMING
  printf("[CONNECT] Time for ibv_reg_mr: %lu \n", ib_mem_reg_ns);
#endif
  //Reset the timer so it can be reused
  TIMER_CLEAR(ib_server_timer);
  
    printd("registered memory region (%lu bytes)\n",
            ib->verbs.mr->length);

    ib->verbs.qp_attr.cap.max_send_wr  = 2;
    ib->verbs.qp_attr.cap.max_send_sge = 2;
    ib->verbs.qp_attr.cap.max_recv_wr  = 2;
    ib->verbs.qp_attr.cap.max_recv_sge = 2;

    ib->verbs.qp_attr.send_cq = ib->verbs.cq;
    ib->verbs.qp_attr.recv_cq = ib->verbs.cq;

    ib->verbs.qp_attr.qp_type = IBV_QPT_RC;

  #ifdef TIMING    
  uint64_t ib_create_qp_ns = 0;
#endif
  TIMER_START(ib_server_timer);
    if (rdma_create_qp(ib->rdma.id, ib->verbs.pd, &ib->verbs.qp_attr))
        return -1;
  
  TIMER_END(ib_server_timer, ib_create_qp_ns);
  
#ifdef TIMING
  printf("[CONNECT] Time for rdma_create_qp: %lu ns\n", ib_create_qp_ns);
#endif

    /* don't need to post a recv... */
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
    pdata.buf_rkey  = htonl(ib->verbs.mr->rkey);
    pdata.buf_va    = htonll((uintptr_t)ib->params.buf);
    pdata.buf_len   = htonll(ib->params.buf_len);
    printd("sending client rkey %u va 0x%llu len %llu\n",
            ib->verbs.mr->rkey, (unsigned long long)ib->params.buf,
            (unsigned long long)ib->params.buf_len);

    ib->rdma.param.responder_resources  = 2;
    ib->rdma.param.private_data         = &pdata;
    ib->rdma.param.private_data_len     = sizeof(pdata);
    ib->rdma.param.initiator_depth      = 2;
    ib->rdma.param.retry_count          = 10;
    ib->rdma.param.rnr_retry_count      = 10;

    printd("accepting connection\n");
    if (rdma_accept(ib->rdma.id, &ib->rdma.param))
        return -1;

    if (rdma_get_cm_event(ib->rdma.ch, &ib->rdma.evt))
        return -1;

    if (ib->rdma.evt->event != RDMA_CM_EVENT_ESTABLISHED)
        return -1;

    rdma_ack_cm_event(ib->rdma.evt);

    return 0;
}
  int
ib_server_disconnect(struct ib_alloc *ib)
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

  #ifdef TIMING    
  uint64_t ib_total_disconnect_ns = 0;
  uint64_t ib_fine_disconnect_ns = 0;
  #endif

  TIMER_DECLARE1(ib_disconnect_timer);
  TIMER_START(ib_disconnect_timer);

  TIMER_DECLARE1(ib_dis_fine_timer);
  TIMER_START(ib_dis_fine_timer);
    //Destroy the queue pair - returns void
    rdma_destroy_qp(ib->rdma.id);
  TIMER_END(ib_dis_fine_timer, ib_fine_disconnect_ns);
  #ifdef TIMING
    printf("[DISCONNECT] Time for rdma_destroy_qp: %lu ns \n", ib_fine_disconnect_ns);
  #endif
  //Reset the timer so it can be reused
  TIMER_CLEAR(ib_dis_fine_timer);

  //------deregister pinned pages---------
  TIMER_START(ib_dis_fine_timer);
  if (ibv_dereg_mr(ib->verbs.mr))
  {
    fprintf(stderr, "failed to deregister MR\n");
    rc = 1;
  }
  TIMER_END(ib_dis_fine_timer, ib_fine_disconnect_ns);
  #ifdef TIMING
    printf("[DISCONNECT] Time for ibv_dereg_mr: %lu ns \n", ib_fine_disconnect_ns);
  #endif
  //Reset the timer so it can be reused
  TIMER_CLEAR(ib_dis_fine_timer);


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

  TIMER_END(ib_disconnect_timer, ib_total_disconnect_ns);

  #ifdef TIMING
    printf("[DISCONNECT] Total time for ib_client_disconnect: %lu ns \n", ib_total_disconnect_ns);
  #endif


  return rc;
}
