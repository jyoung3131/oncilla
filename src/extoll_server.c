/* file: extoll_server.c
 * author: Jeffrey Young, jyoung9@gatech.edu 
 * desc: EXTOLL RMA2 server connect and teardown functions.
 * 
 */

/* System includes */
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

/* Project includes */
#include <io/extoll.h>
#include <util/timer.h>
#include <debug.h>

/* Directory includes */
#include "extoll.h"
#include "extoll_noti.h"

/* Internal definitions */

/* Internal state */

/* Private functions */
static void sighandler(int sig)
{
    printf("Received signal %d - breaking out of the loop\n",sig);
    noti_loop = 0;

    //Jump back to src/extoll.c:extoll_notification initiate EXTOLL teardown
    longjmp(jmp_noti_buf,1);
}

/* Public functions */

//Function copied from RMA2 test code that sets up a buffer region and calls rma2_register on it.
int extoll_server_connect(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;
  int mem_result = 0;

  #ifdef TIMING  
    uint64_t tot_setup_ns = 0;
    uint64_t rma_open_ns = 0;
    uint64_t malloc_ns = 0;
    uint64_t rma_register_ns = 0;
  #endif

  TIMER_DECLARE1(tmp_timer);

  printd("extoll_server_connect:: local_buff_size_B is %lu B\n",ex->params.buf_len);
  //Note that posix_memalign does a malloc, so the buffer should not be allocated yet!

  TIMER_START(tmp_timer);
      rc=rma2_open(&(ex->rma_conn.port));
  TIMER_END(tmp_timer, rma_open_ns);
  TIMER_CLEAR(tmp_timer);

  if (rc!=RMA2_SUCCESS)
  {
    fprintf(stderr,"RMA open failed (%d)\n",rc);
 
    if(rc== RMA2_ERR_IOCTL)
            fprintf(stderr, "Error while communicating with the EXTOLL device driver\n");    
    else if(rc==RMA2_ERR_MMAP)
            fprintf(stderr, "error while trying to mmap dma memory\n");
//    else if(rc==RMA2_ERROR)
  //          fprintf(stderr,"internal error\n");
    else if(rc==RMA2_ERR_NO_DEVICE)
            fprintf(stderr,"no Extoll device found\n");
    else if(rc==RMA2_ERR_PORTS_USED)
            fprintf(stderr,"all endpoint are in use\n");
    else if(rc==RMA2_ERR_FD)
            fprintf(stderr,"opening of /dev/rma2 failed\n");
    else if(rc==RMA2_ERR_INVALID_VERSION)
            fprintf(stderr,"device driver version and API version do not match\n");
    else{
	    fprintf(stderr, "internal error\n");
	}   
    return -1;
  }

  TIMER_START(tmp_timer);
    mem_result=posix_memalign((void**)&(ex->rma_conn.buf),4096,ex->params.buf_len);
  TIMER_END(tmp_timer, malloc_ns);
  TIMER_CLEAR(tmp_timer);

  if (mem_result!=0)
  {
    perror("Memory Buffer allocation failed. Bailing out.");
    return -1;
  }

  //Registration pins the pages in a manner similar to ibv_reg_mr for IB 
  TIMER_START(tmp_timer);
    rc=rma2_register(ex->rma_conn.port, ex->rma_conn.buf, ex->params.buf_len, &(ex->rma_conn.region));
  TIMER_END(tmp_timer, rma_register_ns);

  if (rc!=RMA2_SUCCESS)
  {
    fprintf(stderr,"RMA open failed (%d)\n",rc);
 
    if(rc== RMA2_ERR_IOCTL)
            fprintf(stderr, "Error while communicating with the EXTOLL device driver\n");    
    else if(rc==RMA2_ERR_MMAP)
            fprintf(stderr, "error while trying to mmap dma memory\n");
//    else if(rc==RMA2_ERROR)
  //          fprintf(stderr,"internal error\n");
    else if(rc==RMA2_ERR_NO_DEVICE)
            fprintf(stderr,"no Extoll device found\n");
    else if(rc==RMA2_ERR_PORTS_USED)
            fprintf(stderr,"all endpoint are in use\n");
    else if(rc==RMA2_ERR_FD)
            fprintf(stderr,"opening of /dev/rma2 failed\n");
    else if(rc==RMA2_ERR_INVALID_VERSION)
            fprintf(stderr,"device driver version and API version do not match\n");
    else{
	    fprintf(stderr, "internal error\n");
	}   
    return -1;
  }

  #ifdef TIMING
    tot_setup_ns = rma_open_ns + malloc_ns + rma_register_ns;
    printf("[CONNECT] rma2_open: %lu ns, Malloc mem: %lu ns, Registration: %lu ns, Total Setup: %lu ns\n", rma_open_ns, malloc_ns, rma_register_ns, tot_setup_ns);
  #endif
 
  ex->params.dest_node = rma2_get_nodeid(ex->rma_conn.port);
  ex->params.dest_vpid = rma2_get_vpid(ex->rma_conn.port);
  rma2_get_nla(ex->rma_conn.region, 0, &(ex->params.dest_nla));

  printf("Registered region: node %u vpid %u NLA 0x%lx\n", ex->params.dest_node,  ex->params.dest_vpid,(uint64_t)(ex->params.dest_nla));
  
  return 0;
}

//Capture notifications from RMA put operations
void extoll_server_notification(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;

  /*Catch the Ctrl-\ key combination and jump to the teardown code*/
  signal(SIGQUIT, &sighandler);

  noti_loop = 1;

  printf("Server is waiting for notifications - enter Ctrl-\\ to exit\n");
  while (noti_loop)
  {
    rc=rma2_noti_get_block(ex->rma_conn.port, &(ex->rma_conn.notification));
    //nonblocking version
    //rc=rma2_noti_probe(rma2Obj->port, &(rma2Obj->notification));
    if (rc != RMA2_SUCCESS)
    {
      continue;
    }
#ifdef __DEBUG_ENABLED
#ifndef TIMING
    rma2_noti_dump(ex->rma_conn.notification);
#endif
#endif
    rma2_noti_free(ex->rma_conn.port,ex->rma_conn.notification);
    printd("\n\nContent !=0:\n\n");
  }

}

int extoll_server_disconnect(struct extoll_alloc *ex)
{

  RMA2_ERROR rc;

  TIMER_DECLARE1(teardown_timer);

  #ifdef TIMING
    uint64_t unregister_ns = 0;
    uint64_t rma_close_ns = 0;
    uint64_t teardown_ns = 0;
  #endif

  //Note that disconnect is not needed on this end, since we
  //never performed rma2_connect

  //Unregister the pages when the program is stopped
  printf("Unregister pages\n");
  TIMER_START(teardown_timer);
    rc=rma2_unregister(ex->rma_conn.port, ex->rma_conn.region);
  TIMER_END(teardown_timer, unregister_ns);
  TIMER_CLEAR(teardown_timer);

  if (rc!=RMA2_SUCCESS) 
  {
    print_err(rc);
    return -1;
  }


  printf("Close the RMA port\n");
  ///rma_disconnect(port,handle);
  TIMER_START(teardown_timer);
    rc=rma2_close(ex->rma_conn.port);
  TIMER_END(teardown_timer, rma_close_ns);
  TIMER_CLEAR(teardown_timer);

  if (rc!=RMA2_SUCCESS) 
  {
    print_err(rc);
    return -1;
  }

  #ifdef TIMING
    teardown_ns = unregister_ns + rma_close_ns;
    printf("[DISCONNECT] Unregister pages: %lu ns, rma2_close: %lu ns, Total Teardown: %lu ns\n", unregister_ns, rma_close_ns, teardown_ns);
  #endif

  //free(ex->rma.buf);

  return 0;
}
