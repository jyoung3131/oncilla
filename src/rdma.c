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
#include <util/timer.h>
#include <io/rdma.h>
#include <debug.h>

/* Directory includes */
#include "rdma.h"

/* Globals */

/* Internal definitions */

/* Internal state */

static LIST_HEAD(ib_allocs);

/* Private functions */

/* only used by client code */
static int
post_send(struct ib_alloc *ib, int opcode, size_t src_offset, size_t dest_offset, size_t len)
{
    TIMER_DECLARE1(ib_timer);
    #ifdef TIMING
    uint64_t ib_send_ns = 0;
    #endif

    struct ibv_sge          sge;
    struct ibv_send_wr      wr;
    struct ibv_send_wr      *bad_wr;

    /* "from" address and key */
    if((src_offset+len) > ib->params.buf_len)
    {
      printf("Source offset %lu and send size %lu is larger than buffer length %lu\n",src_offset, len, ib->params.buf_len);
      BUG(1);
    }

    sge.addr   = (uintptr_t)(ib->params.buf+src_offset);
    sge.length = len;
    sge.lkey   = ib->verbs.mr->lkey;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id                = 1 /* ignored: user-defined ID */;
    wr.opcode               = opcode;
    /* This flag is needed so we can poll on send/recv using the Completion
     * Queue data structure. */
    wr.send_flags           = IBV_SEND_SIGNALED;
    wr.sg_list              = &sge;
    wr.num_sge              = 1;
    /* "to" address and key */
    wr.wr.rdma.rkey         = ib->ibv.buf_rkey;
    wr.wr.rdma.remote_addr  = ib->ibv.buf_va + dest_offset;

    TIMER_START(ib_timer);
    if (ibv_post_send(ib->rdma.id->qp, &wr, &bad_wr)){
        perror("ibv_post_send");
        return -1;
    }
    TIMER_END(ib_timer, ib_send_ns);
    TIMER_CLEAR(ib_timer);
    //printf("Time to post %lu bytes: %lu \n", len, ib_send_ns);


    return 0;
}

/* Public functions */

///This function uses socket calls to get the 
///IP address associated with the IB adapter referred
///to by the index, idx
int
ib_nic_ip(int idx, char *ip_str, size_t len)
{
    int fd;
    struct ifreq ifr;

    if (!ip_str)
        return -1;

    len = (len > HOST_NAME_MAX ? HOST_NAME_MAX : len);

    //Create a UDP / Datagram socket
    if (0 > (fd = socket(AF_INET, SOCK_DGRAM, 0))) {
        printd("invalid socket returned\n");
        return -1;
    }
    ifr.ifr_addr.sa_family = AF_INET; /* IPv4 */
    snprintf(ifr.ifr_name, IFNAMSIZ - 1, "ib%d", idx);
    //strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    if (ioctl(fd, SIOCGIFADDR, &ifr)) {
        close(fd);
        printd("ioctl error\n");
        return -1;
    }
    close(fd);

    //This line requires that the optimized version of Oncilla be compiled with -fno-strict-aliasing
    strncpy(ip_str, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr), len);
    ip_str[len-1] = '\0'; /* just in case */
    return 0;
}

int
ib_init(void)
{
    /* TODO in case we need to add init stuff later */
    return 0;
}

ib_t
ib_new(struct ib_params *p)
{
    struct ib_alloc *ib = NULL;

    if (!p)
        goto fail;

    ib = calloc(1, sizeof(*ib));
    if (!ib)
        goto fail;

    if (p->addr) /* only client specifies this */
        ib->params.addr = strdup(p->addr);
    memcpy(&ib->params, p, sizeof(*p));

    /* TODO Lock this list */
    INIT_LIST_HEAD(&ib->link);
    list_add(&ib->link, &ib_allocs);

    return (ib_t)ib;

fail:
    return (ib_t)NULL;
}

//Free the IB allocation object
int
ib_free(ib_t ib)
{
    int ret = 0;

    //Free the buffer in the ib->params struct
    if(ib->params.buf)
      free(ib->params.buf);

    //Delete the IB object from the list
    list_del(&(ib->link));

    //Free the IB object
    free(ib);
    return ret;

}



/* TODO provide an accept and connect separately, instead of the bool */
int
ib_connect(ib_t ib, bool is_server)
{
    int err;

    if (!ib)
        return -1;

    if (is_server)
        err = ib_server_connect((struct ib_alloc*)ib);
    else
        err = ib_client_connect((struct ib_alloc*)ib);

    return err;
}

int
ib_disconnect(ib_t ib, bool is_server)
{
    int err;

    if (!ib)
        return -1;

    if (is_server)
        err = ib_server_disconnect((struct ib_alloc*)ib);
    else
        err = ib_client_disconnect((struct ib_alloc*)ib);

    return err;
}



int
ib_reg_mr(ib_t ib, void *buf, size_t len)
{
    if (!ib || !buf || len == 0)
        return -1;

    printf(">> registering memory\n");

    /* overwrite existing values */
    ib->params.buf      = buf;
    ib->params.buf_len  = len;

    ib->verbs.mr = ibv_reg_mr(ib->verbs.pd, buf, len,
            IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
            IBV_ACCESS_REMOTE_WRITE);

    if(!ib->verbs.mr)
        return -1; 

    return 0;
}

/* TODO Include specifying segments to send, instead of static offsets.  Like,
 * enqueue a write, then flush the writes
 */

/* client function: pull data fom server */
int
ib_read(ib_t ib, size_t src_offset, size_t dest_offset, size_t len)
{
    if (!ib)
        return -1;
    if ((dest_offset + len) > ib->ibv.buf_len) {
        printd("error: would read past end of remote buffer\n");
        return -1;
    }
    return post_send(ib, IBV_WR_RDMA_READ, src_offset, dest_offset, len);
}

/* client function: push data to server */
int
ib_write(ib_t ib, size_t src_offset, size_t dest_offset, size_t len)
{
    if (!ib || len == 0)
        return -1;
    if ((dest_offset + len) > ib->ibv.buf_len) {
        printd("error: would write past end of remote buffer\n");
        return -1;
    }
    return post_send(ib, IBV_WR_RDMA_WRITE, src_offset, dest_offset, len);
}

/* Wait for some event. Code found in manpage of ibv_get_cq_event */
int
ib_poll(ib_t ib)
{
    struct ibv_wc   wc;
    struct ibv_cq   *evt_cq;
    void            *cq_ctxt;
    int             ne;
    TIMER_DECLARE1(ib_timer);
    #ifdef TIMING
    uint64_t ib_send_ns = 0;
    #endif

    TIMER_START(ib_timer);

    if (ibv_req_notify_cq(ib->verbs.cq, 0))
        return -1;

    if (ibv_get_cq_event(ib->verbs.ch, &evt_cq, &cq_ctxt))
        return -1;

    ibv_ack_cq_events(evt_cq, 1);

    if (ibv_req_notify_cq(evt_cq, 0))
        return -1;

    do {
        ne = ibv_poll_cq(ib->verbs.cq, 1, &wc);

        if (ne == 0)
        {
            continue;
        }
        else if (ne < 0)
            return -1;

        if (wc.status != IBV_WC_SUCCESS)
            return -1;

    } while (ne);
    TIMER_END(ib_timer, ib_send_ns);
    TIMER_CLEAR(ib_timer);
    
    //printf("Time to poll bytes: %lu \n", ib_send_ns);

    return 0;
}

/* TODO server functions */
/* right now the server is stupid, just helps make memory then goes away */
