/**
 * file: extoll.h
 * author: Jeff Young jyoung9@gatech.edu
 * desc: interface for managing EXTOLL allocations and data movement
 */

#ifndef __EXTOLL_H__
#define __EXTOLL_H__

/* System includes */
#include <stdbool.h>
#include <stdint.h>

/* Other project includes */
//RMA2 header files
#include "/extoll2/include/rma2.h"
#include "/extoll2/include/pmap.h"

/* Project includes */

/* Defines */

/* Types */

struct extoll_alloc; /* forward declaration */
typedef struct extoll_alloc * extoll_t;

//extoll_params contains all the information needed to set up/disconnect
//an RMA2 connection. The __rma_t struct in src/extoll.h contains the
//variables used to manage an existing connection.
struct extoll_params {
  //Specify the remote node and VPID (similar to port) that is the
  //target of put/get operations. 
  RMA2_Nodeid dest_node; //uint16_t
  RMA2_VPID dest_vpid;   //uint16_t
  //Network Level Address is used as a kind of offset for the hardware
  //to specify the destination for put/get operations.
  RMA2_NLA dest_nla; //uint64_t
  //A void pointer to pages that are pinned and can be associated
  //with an RMA2_Region
  void* buf;
  //Buffer size - specified in terms of Bytes (not ints as IB is)
  size_t buf_len;
};

/* Global state (externs) */

/* Function prototypes */

int extoll_init(void);
extoll_t extoll_new(struct extoll_params *p);
int extoll_free(extoll_t ex);
int extoll_connect(extoll_t ex, bool is_server);
//Wait for notifications on server - typically used with put 
//operations (client handles get notifications)
void extoll_notification(extoll_t ex);
int extoll_disconnect(extoll_t ex, bool is_server);
int extoll_read(extoll_t ex, size_t src_offset, size_t dest_offset, size_t len);
int extoll_write(extoll_t ex, size_t src_offset, size_t dest_offset, size_t len);

static void print_err(RMA2_ERROR err)
{
    fprintf(stderr, "RMA error occured: %x\n", (unsigned int )err);
}


/* TODO include func to change remote mapping of local buf */

#endif  /* __EXTOLL_H__ */
