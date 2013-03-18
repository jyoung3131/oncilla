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

/* Project includes */
#include <io/extoll.h>
#include <debug.h>

/* Directory includes */
#include "extoll.h"

/* Internal definitions */

/* Internal state */

/* Private functions */

/* Public functions */

//Function copied from RMA2 test code that sets up a buffer region and calls rma2_register on it.
int extoll_server_connect(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;
  int mem_result = 0;

  printf("extoll_server_connect:: local_buff_size_B is %lu B\n",ex->params.buf_len);
  //Note that posix_memalign does a malloc, so the buffer should not be allocated yet!

      rc=rma2_open(&(ex->rma.port));

  if (rc!=RMA2_SUCCESS)
  {
    fprintf(stderr,"RMA open failed (%d)\n",rc);
    return -1;
  }

    mem_result=posix_memalign((void**)&(ex->rma.buf),4096,ex->params.buf_len);

  if (mem_result!=0)
  {
    perror("Memory Buffer allocation failed. Bailing out.");
    return -1;
  }

  //Registration pins the pages in a manner similar to ibv_reg_mr for IB 
    rc=rma2_register(ex->rma.port, ex->rma.buf, ex->params.buf_len, &(ex->rma.region));

  if (rc!=RMA2_SUCCESS)
  {
    fprintf(stderr,"Error while registering memory. Bailing out!\n");
    return -1;
  }

  ex->params.dest_node = rma2_get_nodeid(ex->rma.port);
  ex->params.dest_vpid = rma2_get_vpid(ex->rma.port);
  rma2_get_nla(ex->rma.region, 0, &(ex->params.dest_nla));


  printf("Registered region: node %u vpid %u NLA 0x%lx\n", ex->params.dest_node,  ex->params.dest_vpid,(uint64_t)(ex->params.dest_nla));
  
  return 0;
}

//Capture notifications from RMA put operations
void extoll_server_notification(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;

  printf("Server is waiting for notifications - enter Ctrl-D to exit\n");
  char d;
  while ((d=getchar())!=EOF)
  {
    rc=rma2_noti_get_block(ex->rma.port, &(ex->rma.notification[0]));
    if (rc != RMA2_SUCCESS)
    {
      continue;
    }
    rma2_noti_dump(ex->rma.notification[0]);
    rma2_noti_free(ex->rma.port,ex->rma.notification[0]);
    printf("\n\nContent !=0:\n\n");
  }

}

int extoll_server_disconnect(struct extoll_alloc *ex)
{

  RMA2_ERROR rc;

  //Note that disconnect is not needed on this end, since we
  //never performed rma2_connect

  //Unregister the pages when the program is stopped
  printf("Unregister pages\n");
    rc=rma2_unregister(ex->rma.port, ex->rma.region);

  if (rc!=RMA2_SUCCESS) 
  {
    print_err(rc);
    return -1;
  }


  printf("Close the RMA port\n");
  ///rma_disconnect(port,handle);
    rc=rma2_close(ex->rma.port);

  if (rc!=RMA2_SUCCESS) 
  {
    print_err(rc);
    return -1;
  }

  free(ex->rma.buf);

  return 0;
}