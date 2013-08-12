/* file: extoll_client.c
 * author: Jeff Young, jyoung9@gatech.edu
 * desc: EXTOLL RMA2 client setup and teardown
 * 
 */

/* System includes */
#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>

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
int extoll_client_connect(struct extoll_alloc *ex, ocm_timer_t tm)
{
  RMA2_ERROR rc;
  int mem_result;

  TIMER_DECLARE1(setup_timer);

  printf("Setting up remote memory connection to node %d, vpid %d, and 0x%lx NLA with RMA2\n", ex->params.dest_node, ex->params.dest_vpid, ex->params.dest_nla);

  TIMER_START(setup_timer);
  mem_result=posix_memalign((void**)&(ex->rma_conn.buf),4096,ex->params.buf_len);
  TIMER_END(setup_timer, tm->alloc_tm.rma.malloc_ns);
  TIMER_CLEAR(setup_timer);

  if (mem_result!=0)
  {
    perror("Memory Buffer allocation failed. Bailing out.");
    return -1;
  }

  memset(ex->rma_conn.buf, 0, ex->params.buf_len);
  printd("Region starts at %p\n", ex->rma_conn.buf);

  printf("Opening port\n");
  TIMER_START(setup_timer);
  rc=rma2_open(&(ex->rma_conn.port));
  TIMER_END(setup_timer, tm->alloc_tm.rma.open_ns);
  TIMER_CLEAR(setup_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  //Must connect to the remote node for put/get operations
  TIMER_START(setup_timer);
  rc = rma2_connect(ex->rma_conn.port, ex->params.dest_node, ex->params.dest_vpid, ex->rma_conn.conn_type, &(ex->rma_conn.handle));
  TIMER_END(setup_timer, tm->alloc_tm.rma.conn_ns);
  TIMER_CLEAR(setup_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }
  printf("Testing buffer\n");

  //uint32_t* buf = (uint32_t*)ex->rma_conn.buf;
  //buf[0] = 2;
  //buf[3] = 4;
  //printf("The value of buf is %d \n", buf[3]);

  printd("Registering with remote memory\n");
  //register pins the memory and associates it with an RMA2_Region
  TIMER_START(setup_timer);
  rc=rma2_register(ex->rma_conn.port, ex->rma_conn.buf, ex->params.buf_len, &(ex->rma_conn.region));
  TIMER_END(setup_timer, tm->alloc_tm.rma.reg_ns);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

#ifdef TIMING
  tm->tot_setup_ns = tm->alloc_tm.rma.malloc_ns + tm->alloc_tm.rma.open_ns + tm->alloc_tm.rma.conn_ns + tm->alloc_tm.rma.reg_ns;
  printf("[CONNECT] malloc: %lu ns, open: %lu ns, connect : %lu ns, register %lu ns, total setup: %lu ns\n", tm->alloc_tm.rma.malloc_ns, tm->alloc_tm.rma.open_ns, tm->alloc_tm.rma.conn_ns, tm->alloc_tm.rma.reg_ns, tm->tot_setup_ns);
#endif

  return 0;
}

int extoll_client_disconnect(struct extoll_alloc *ex, ocm_timer_t tm)
{
  RMA2_ERROR rc;
  TIMER_DECLARE1(teardown_timer);

  printf("RMA2 disconnect\n");
  TIMER_START(teardown_timer);
  rc=rma2_disconnect(ex->rma_conn.port,ex->rma_conn.handle);
  TIMER_END(teardown_timer, tm->alloc_tm.rma.discon_ns);
  TIMER_CLEAR(teardown_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  TIMER_START(teardown_timer);
  rc=rma2_unregister(ex->rma_conn.port, ex->rma_conn.region);
  TIMER_END(teardown_timer, tm->alloc_tm.rma.dereg_ns);
  TIMER_CLEAR(teardown_timer);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }


  printf("Close the RMA port\n");

  TIMER_START(teardown_timer);
  rc=rma2_close(ex->rma_conn.port);
  TIMER_END(teardown_timer, tm->alloc_tm.rma.close_ns);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

#ifdef TIMING
  tm->tot_teardown_ns = tm->alloc_tm.rma.discon_ns + tm->alloc_tm.rma.dereg_ns + tm->alloc_tm.rma.close_ns;
  printf("[DISCONNECT] Disconnect: %lu ns, Unregister: %lu ns, rma2_close: %lu ns, Total Teardown: %lu ns\n", tm->alloc_tm.rma.discon_ns, tm->alloc_tm.rma.dereg_ns, tm->alloc_tm.rma.close_ns, tm->tot_teardown_ns);
#endif

  //Free the memory region and associated buffer
  //free(ex->rma.region);
  return 0;
}

