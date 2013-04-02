/* file: extoll.c
 * author: Jeff Young <jyoung9@gatech.edu>
 * desc: EXTOLL RMA2 helper functions
 *
 * The list of allocs in this file will either be all server-side if the process
 * is a daemon, or all client-side if the process is the application (i.e. this
 * file is within the ocm library).
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <limits.h>

/* Project includes */
#include <util/list.h>
#include <io/extoll.h>
#include <debug.h>

/* Directory includes */
#include "extoll.h"
#include "extoll_noti.h"

/* Globals */

/* Internal definitions */

/* Internal state */

static LIST_HEAD(extoll_allocs);
//Run the notification only once
static int run_once;

/* Private functions */

//put_get_flag: put = 0; get = 1
int extoll_rma2_transfer(extoll_t ex, size_t put_get_flag, size_t src_offset, size_t dest_offset, size_t len)
{
  RMA2_ERROR rc;

  //The number of notifications specifies how many overlapping put/get operations we can 
  //**NOTE** Testing showed that increasing the number of notifications doesn't have a great impact
  //on bandwidth so 2 is the default number of overlapping calls
  uint32_t num_notis = 2;
  uint32_t i;
  //**NOTE: This command can transfer up to 8 MB at once, so larger transfers must be broken down into smaller transfers
  //For the last transfer we may need to transfer a small amount of data for the second put/get
  uint64_t max_num_B_per_call = 8*(2<<20);

  uint32_t last_transfer_flag = 0;
  uint32_t last_num_transfers = 0;

  uint64_t *src_addr = (uint64_t*)calloc(num_notis,sizeof(uint64_t));
  uint64_t *dest_addr = (uint64_t*)calloc(num_notis,sizeof(uint64_t));

  //Initialize the destination and source offset and check to make sure that we aren't
  //reading or writing past the local or remote buffer
  ex->rma.dest_offset = dest_offset;
  ex->rma.src_offset = src_offset;

  printd("RMA2 data transfer - need to transfer %lu B in 8 MB chunks\n", len);
  printd("Up to %d overlapping put/get operations are allowed\n", num_notis);

  while(len > 0)
  {
    //Each time through the loop update the base address to put/get to/from and then update
    //the address for each additional put/get call
    dest_addr[0] = ex->params.dest_nla+ex->rma.dest_offset;
    src_addr[0] = ex->rma.src_offset;

    for(i = 1; i < num_notis; i++)
    {
      dest_addr[i] = dest_addr[i-1] + max_num_B_per_call;
      src_addr[i]  = src_addr[i-1] + max_num_B_per_call;
    }

    //Issue N overlapping puts (0) or gets (1) to increase bandwidth
    if(put_get_flag == 0)
    {
      for(i = 0; ((i < num_notis) && (!last_transfer_flag)); i++)
      {
        //Check first for the last transfer (one smaller than 8 MB)
        if(len < max_num_B_per_call)
        {
          if(len == 0)
          {
            last_transfer_flag = 1;
            break;
          }
          last_transfer_flag = 1;
          max_num_B_per_call = len;
          len = 0;
          last_num_transfers = i+1;

        }
        else
          len -= max_num_B_per_call;

        //For put, RMA2_COMPLETER_NOTIFICATION indicates the the put command has completed (write has finished in remote memory)
        rc=rma2_post_put_bt(ex->rma.port,ex->rma.handle,ex->rma.region, src_addr[i], max_num_B_per_call,dest_addr[i],RMA2_ALL_NOTIFICATIONS,RMA2_CMD_DEFAULT);
        //rc=rma2_post_put_bt(ex->rma.port,ex->rma.handle,ex->rma.region, src_addr[i], max_num_B_per_call,dest_addr[i],RMA2_COMPLETER_NOTIFICATION | RMA2_REQUESTER_NOTIFICATION,RMA2_CMD_DEFAULT);

      }//end for
    }//end if put
    else
    {
      for(i = 0; ((i < num_notis) && (!last_transfer_flag)); i++)
      {

        printd("Num B left %lu i %d num_notis %d\n",len, i, num_notis);
        //Check first for the last transfer (one smaller than 8 MB)
        if(len < max_num_B_per_call)
        {
          if(len == 0)
          {
            last_transfer_flag = 1;
            break;
          }

          last_transfer_flag = 1;
          max_num_B_per_call = len;
          len = 0;
          last_num_transfers = i+1;
        }
        else
          len -= max_num_B_per_call;
        
        //For put, RMA2_COMPLETER_NOTIFICATION indicates the the put command has completed (write has finished in remote memory)
        rc=rma2_post_get_bt(ex->rma.port,ex->rma.handle,ex->rma.region, src_addr[i], max_num_B_per_call,dest_addr[i],RMA2_COMPLETER_NOTIFICATION,RMA2_CMD_DEFAULT);

        //Increment the dest and source offset for each put or get
        ex->rma.dest_offset += max_num_B_per_call;
        ex->rma.src_offset += max_num_B_per_call;
      }//end for
    }

    //One the last sequence of transfers there may be fewer outstanding notifications
    if(last_transfer_flag)
      num_notis = last_num_transfers;

    //Wait for N notifications that the data was transferred
      for(i=0;i<num_notis;i++)
      {
        printd("Notis i %d num_notis %d\n", i, num_notis);
        //By timing the blocking time we get a full picture of when the put/get operation completed
        rc=rma2_noti_get_block(ex->rma.port, &(ex->rma.notification));

        if (rc!=RMA2_SUCCESS)
        {
          fprintf(stderr,"error in rma2_noti_get_block\n");
          return 1;
        }
        printd("\nGot Notification:\n");
        printd("-------------------------\n");
        //rma2_noti_dump just prints out the notification so it is not neccessarily needed
        //Diable by default; check inc/debug.h on how to enable
#ifdef __DEBUG_ENABLED  
        rma2_noti_dump(ex->rma.notification);
#endif
        //But notifications must be freed to process new notifications
        rma2_noti_free(ex->rma.port,ex->rma.notification);
        printd("-------------------------\n");
      }
  }//end while

  free(src_addr);
  free(dest_addr);

  return 0;
}


/* Public functions */

  int
extoll_init(void)
{
  /* TODO in case we need to add init stuff later */
  return 0;
}

  extoll_t
extoll_new(struct extoll_params *p)
{
  struct extoll_alloc *ex = NULL;

  if (!p)
    goto fail;

  ex = calloc(1, sizeof(*ex));
  if (!ex)
    goto fail;

  //Store the destination node ID, VPID, and NLA
  memcpy(&ex->params, p, sizeof(*p));

  /* TODO Lock this list */
  INIT_LIST_HEAD(&ex->link);
  list_add(&ex->link, &extoll_allocs);

  return (extoll_t)ex;

fail:
  return (extoll_t)NULL;
}

//Free the EXTOLL allocation object
  int
extoll_free(extoll_t ex)
{
  int ret = 0;

  //The buffer params.buf should be freed when
  //pages are unregistered in extoll_client_disconnect)

  //Delete the EXTOLL object from the list
  list_del(&(ex->link));

  //Free the EXTOLL object
  free(ex);
  return ret;

}

void extoll_notification(extoll_t ex)
{
  //Store this location so we can jump back here if needed to
  //break out of the notification call
  setjmp(jmp_noti_buf);
  //Once we jump back the notification call will not be run the second time
  if(!run_once)
  {
    run_once = 1;
    extoll_server_notification((struct extoll_alloc*)ex);
  }
}

//Close down the EXTOLL server and client applications
  int
extoll_disconnect(extoll_t ex, bool is_server)
{
  int err;

  if (!ex)
    return -1;

  if (is_server)
    err = extoll_server_disconnect((struct extoll_alloc*)ex);
  else
    err = extoll_client_disconnect((struct extoll_alloc*)ex);

  return err;
}



/* TODO provide an accept and connect separately, instead of the bool */
  int
extoll_connect(extoll_t ex, bool is_server)
{
  int err;

  if (!ex)
    return -1;

  if (is_server)
    err = extoll_server_connect((struct extoll_alloc*)ex);
  else
    err = extoll_client_connect((struct extoll_alloc*)ex);

  return err;
}

/* client function: pull data fom server */
  int
extoll_read(extoll_t ex, size_t src_offset, size_t dest_offset, size_t len)
{
  if (!ex)
    return -1;
  if ((dest_offset + len) > ex->params.buf_len) {
    printd("error: would read past end of remote buffer\n");
    return -1;
  }
  return extoll_rma2_transfer(ex, 1, src_offset, dest_offset, len);

}

/* client function: push data to server */
  int
extoll_write(extoll_t ex, size_t src_offset, size_t dest_offset, size_t len)
{
  if (!ex || len == 0)
    return -1;
  if ((dest_offset + len) > ex->params.buf_len) {
    printd("error: would write past end of remote buffer\n");
    return -1;
  }
  return extoll_rma2_transfer(ex, 0, src_offset, dest_offset, len);
}

