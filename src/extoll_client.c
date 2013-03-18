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
#include <io/extoll.h>
#include <util/timer.h>
#include <debug.h>

/* Directory includes */
#include "extoll.h"

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */
int extoll_client_connect(struct extoll_alloc *ex)
{
 RMA2_ERROR rc;
  #ifdef TIMING
    uint64_t open_ns = 0;
    uint64_t malloc_ns = 0;
    uint64_t connect_ns = 0;
    uint64_t register_ns = 0;
    uint64_t total_setup_ns = 0;
  #endif

   TIMER_DECLARE1(setup_timer);

  printf("Setting up remote memory connection to node %d, vpid %d, and 0x%lx NLA with RMA2\n", ex->params.dest_node, ex->params.dest_vpid, ex->params.dest_nla);

  TIMER_START(setup_timer);
    ex->rma.buf = (void*)malloc(ex->params.buf_len);
  TIMER_END(setup_timer, malloc_ns);
  TIMER_CLEAR(setup_timer);
  memset(ex->rma.buf, 0, ex->params.buf_len);
  printf("Region starts at %p\n", ex->rma.buf);

  printf("Opening port\n");
  TIMER_START(setup_timer);
    rc=rma2_open(&(ex->rma.port));
  TIMER_END(setup_timer, open_ns);
  TIMER_CLEAR(setup_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  //Must connect to the remote node for put/get operations
  TIMER_START(setup_timer);
    rma2_connect(ex->rma.port, ex->params.dest_node, ex->params.dest_vpid, ex->rma.conn_type, &(ex->rma.handle));
  TIMER_END(setup_timer, connect_ns);
  TIMER_CLEAR(setup_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  printf("Registering with remote memory\n");
  //register pins the memory and associates it with an RMA2_Region
  TIMER_START(setup_timer);
    rc=rma2_register(ex->rma.port, ex->rma.buf, ex->params.buf_len, &(ex->rma.region));
  TIMER_END(setup_timer, register_ns);
  
  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  #ifdef TIMING
    total_setup_ns = malloc_ns + open_ns + connect_ns + register_ns;
    printf("[CONNECT] malloc: %lu ns, open: %lu ns, connect : %lu ns, register %lu ns, total setup: %lu ns\n", malloc_ns, open_ns, connect_ns, register_ns, total_setup_ns);
  #endif

  return 0;
}

int extoll_client_disconnect(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;
  TIMER_DECLARE1(teardown_timer);

  #ifdef TIMING
    uint64_t disconnect_ns = 0;
    uint64_t rma_close_ns = 0;
  #endif

  printf("RMA2 disconnect\n");
  TIMER_START(teardown_timer);
    rc=rma2_disconnect(ex->rma.port,ex->rma.handle);
  TIMER_END(teardown_timer, disconnect_ns);
  TIMER_CLEAR(teardown_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  printf("Close the RMA port\n");
  TIMER_START(teardown_timer);
    rc=rma2_close(ex->rma.port);
  TIMER_END(teardown_timer, rma_close_ns);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  #ifdef TIMING
    printf("[DISCONNECT] Disconnect: %lu ns, rma2_close: %lu ns, Total Teardown: %lu ns\n", disconnect_ns, rma_close_ns, disconnect_ns + rma_close_ns);
  #endif

  //Free the memory region and associated buffer
  free(ex->rma.region);

  return 0;
}

