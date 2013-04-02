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

/* Project includes */
#include <io/extoll.h>
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

  printf("Setting up remote memory connection to node %d, vpid %d, and 0x%lx NLA with RMA2\n", ex->params.dest_node, ex->params.dest_vpid, ex->params.dest_nla);

  ex->rma_conn.buf = (void*)malloc(ex->params.buf_len);
  memset(ex->rma_conn.buf, 0, ex->params.buf_len);
  printd("Region starts at %p\n", ex->rma_conn.buf);

  printd("Opening port\n");
  rc=rma2_open(&(ex->rma_conn.port));

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  //Must connect to the remote node for put/get operations
  rc = rma2_connect(ex->rma_conn.port, ex->params.dest_node, ex->params.dest_vpid, ex->rma_conn.conn_type, &(ex->rma_conn.handle));

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  printd("Registering with remote memory\n");
  //register pins the memory and associates it with an RMA2_Region
  rc=rma2_register(ex->rma_conn.port, ex->rma_conn.buf, ex->params.buf_len, &(ex->rma_conn.region));

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  return 0;
}

int extoll_client_disconnect(struct extoll_alloc *ex)
{
  RMA2_ERROR rc;

  printd("Unregister pages\n");
  rc=rma2_unregister(ex->rma_conn.port, ex->rma_conn.region);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  printd("RMA2 disconnect\n");
  rc=rma2_disconnect(ex->rma_conn.port,ex->rma_conn.handle);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }

  printf("Close the RMA port\n");
  rc=rma2_close(ex->rma_conn.port);

  if (rc!=RMA2_SUCCESS) 
  { 
    print_err(rc);
    return -1;
  }


  return 0;
}

