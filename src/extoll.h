/* file: extoll.h
 * author: Jeff Young <jyoung9@gatech.edu>
 * desc: internal data structures for the EXTOLL interface
 *
 */

#ifndef __EXTOLL_INTERNAL_H__
#define __EXTOLL_INTERNAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//Used to handle a special case where the notification blocking
//statement cannot be broken out of
#include <setjmp.h>
//Create a signal handler to break out of notification
#include <signal.h>
#include <sys/types.h>

#include <fcntl.h>                                             
#include <sys/mman.h>                                           
#include <sys/ioctl.h>                                           
#include "/extoll2/include/pmap.h"
#include "/extoll2/include/rma2.h"

#include <util/list.h>

struct __rma_t {
  //An RMA port is a pointer to an RMA_Endpoint struct that contains
  //information about the RMA connection.
  RMA2_Port port;
  //Create a pointer to an RMA region that holds information about
  //pinned memory pages that can be used as a source/destination for
  //get/put operations
  RMA2_Region* region;
  //Notifications specify that a particular operation (typically a
  //put or get) has completed within the EXTOLL NIC hardware and that
  //memory referred to by this operation can be safely used
  //For best bandwidth, two put/get operations can be overlapped, requiring
  //two notifications
  RMA2_Notification* notification;
  //Handles hold information on connections
  RMA2_Handle handle;
  //The connection type specifies whether the RMA connection is directly
  //accessing memory, registers, or using API-related structures
  RMA2_Connection_Options conn_type;
  //Local offset used for data transfers
  uint64_t src_offset;
  //Remote offset used for data transfers
  uint64_t dest_offset;
  //A void pointer to pages that are pinned and can be associated
  //with an RMA2_Region
  void* buf;
};

struct extoll_alloc
{
    struct list_head    link;
    struct __rma_t rma;
    struct extoll_params params;
};

/* server functions */
int extoll_server_connect(struct extoll_alloc *ex_alloc);
void extoll_server_notification(struct extoll_alloc *ex_alloc);
int extoll_server_disconnect(struct extoll_alloc *ex_alloc);

/* client functions */
int extoll_client_connect(struct extoll_alloc *ex_alloc);
int extoll_client_disconnect(struct extoll_alloc *ex_alloc);

#endif
