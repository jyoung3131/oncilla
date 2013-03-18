/* file: rdma.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: InfiniBand RDMA helper functions and threading code
 *
 * The list of allocs in this file will either be all server-side if the process
 * is a daemon, or all client-side if the process is the application (i.e. this
 * file is within the ocm library).
 */

/* System includes */
#include <arpa/inet.h>
#include <infiniband/arch.h>
#include <infiniband/verbs.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <limits.h>
/* for ib_nic_ip */
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

/* Project includes */
#include <util/list.h>
#include <io/extoll.h>
#include <debug.h>

/* Directory includes */
#include "extoll.h"

/* Globals */

/* Internal definitions */

/* Internal state */

static LIST_HEAD(extoll_allocs);

/* Private functions */

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
    //ex->params.dest_node = p->dest_node;
    //ex->params.dest_vpid = p->dest_vpid;
    //ex->params.dest_nla = p->dest_nla;
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

    //Free the buffer in the ex->params struct
    if(ex->params.buf)
      free(ex->params.buf);

    //Delete the EXTOLL object from the list
    list_del(&(ex->link));

    //Free the EXTOLL object
    free(ex);
    return ret;

}

void extoll_notification(extoll_t ex)
{
  extoll_server_notification((struct extoll_alloc*)ex);
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
extoll_read(extoll_t ex, size_t offset, size_t len)
{
    if (!ex)
        return -1;
    if ((offset + len) > ex->buf_len) {
        printd("error: would read past end of remote buffer\n");
        return -1;
    }
    return extoll_transfer(ex, 0, offset, len);
}

/* client function: push data to server */
int
extoll_write(extoll_t ex, size_t offset, size_t len)
{
    if (!ex || len == 0)
        return -1;
    if ((offset + len) > ib->ibv.buf_len) {
        printd("error: would write past end of remote buffer\n");
        return -1;
    }
    return post_send(ib, IBV_WR_RDMA_WRITE, offset, len);
}

//ib_reg_mr(ib_t ex, void *buf, size_t len)

/* client function: pull data fom server */
/*int
ib_read(ib_t ib, size_t offset, size_t len)*/

//int ib_write(ib_t ib, size_t offset, size_t len)
