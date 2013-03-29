/**
 * file: lib.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: library apps link with; libocm.so and oncillamem.h
 */

/* System includes */
#include <stdio.h>
#include <pthread.h>
#include <string.h>

/* Other project includes */

/* Project includes */
#include <oncillamem.h>
#include <pmsg.h>
#include <msg.h>
#include <debug.h>
#include <alloc.h>

/* Directory includes */
#include <cuda.h>
#include <cuda_runtime.h> 

/* Globals */

/* Internal definitions */

struct lib_alloc {
    struct list_head link;
    enum ocm_kind kind;
    /* TODO Later, when allocations are composed of partitioned distributed
     * allocations, this will no longer be a single union, but an array of them,
     * to accomodate the heterogeneity in allocations.
     */
    union {
        #ifdef INFINIBAND
        struct {
            ib_t ib;
            int remote_rank;
            size_t remote_bytes;
            size_t local_bytes;
            void *local_ptr;
        } rdma;
        #endif
        #ifdef EXTOLL
        struct {
            /* TODO */
        } rma;
        #endif
        struct {
            size_t bytes;
            void *ptr;
        } local;
        /* TODO GPU? Not sure where that would fit. */
    } u;
};


/* Internal state */

static LIST_HEAD(allocs); /* list of lib_alloc */
static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;

#define for_each_alloc(alloc, allocs) \
    list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()   pthread_mutex_lock(&allocs_lock)
#define unlock_allocs() pthread_mutex_unlock(&allocs_lock)

/* Private functions */

/* Global functions */

int
ocm_init(void)
{
    struct message msg;
    int tries = 10; /* to open daemon mailbox */
    bool opened = false, attached = false;
    int ret = -1;

    /* open resources */
    if (pmsg_init(sizeof(struct message)))
        goto out;
    if (pmsg_open(getpid()))
        goto out;
    opened = true;
    while ((usleep(10000), tries--) > 0)
        if (0 == pmsg_attach(PMSG_DAEMON_PID))
            break;
    if (tries <= 0)
        goto out;
    attached = true;

    #ifdef INFINIBAND
    if (ib_init()) {
        goto out;
    }
    #endif

    /* tell daemon who we are, wait for confirmation msg */
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_CONNECT;
    msg.pid = getpid();
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
        goto out;
    if (pmsg_recv(&msg, true))
        goto out;
    if (msg.type != MSG_CONNECT_CONFIRM)
        goto out;
    ret = 0;
    
out:
    printd("attach to daemon: %s\n", (ret ? "fail" : "success"));
    if (ret) {
        if (attached)
            pmsg_detach(PMSG_DAEMON_PID);
        if (opened)
            pmsg_close();
    }
    return ret;
}

int
ocm_tini(void)
{
    struct message msg;
    int ret = -1;
    msg.type = MSG_DISCONNECT;
    msg.pid = getpid();
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
        goto out;
    if (pmsg_detach(PMSG_DAEMON_PID))
        goto out;
    if (pmsg_close())
        goto out;
    ret = 0;
out:
    printd("detach from daemon: %s\n", (ret ? "fail" : "success"));
    return ret;
}

ocm_alloc_t
ocm_alloc(size_t bytes, enum ocm_kind kind)
{
    struct message msg;
    struct lib_alloc *alloc;
    int ret = -1;

    alloc = calloc(1, sizeof(*alloc));
    if (!alloc)
        goto out;

    msg.type        = MSG_REQ_ALLOC;
    msg.status      = MSG_REQUEST;
    msg.pid         = getpid();
    msg.u.req.bytes = bytes;

    if (kind == OCM_LOCAL_HOST)
        msg.u.req.type = ALLOC_MEM_HOST;
    else if(kind == OCM_LOCAL_GPU)
        msg.u.req.type = ALLOC_MEM_GPU;
    else if (kind == OCM_REMOTE_RDMA)
        msg.u.req.type = ALLOC_MEM_RDMA;
    else if (kind == OCM_REMOTE_RMA)
        msg.u.req.type = ALLOC_MEM_RMA;
    else
        goto out;
    
    printd("sending req_alloc to daemon\n");
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
        goto out;

    printd("waiting for reply from daemon\n");
    if (pmsg_recv(&msg, true))
        goto out;
    BUG(msg.type != MSG_RELEASE_APP);

    if (msg.u.alloc.type == ALLOC_MEM_HOST) {
        printd("ALLOC_MEM_HOST %lu bytes\n", msg.u.alloc.bytes);
        alloc->kind             = OCM_LOCAL_HOST;
        alloc->u.local.bytes    = msg.u.alloc.bytes;
        alloc->u.local.ptr      = malloc(msg.u.alloc.bytes);
        if (!alloc->u.local.ptr)
            goto out;
    }
    #ifdef CUDA
    else if (msg.u.alloc.type == ALLOC_MEM_GPU) {
        printd("ALLOC_MEM_GPU %lu bytes\n", msg.u.alloc.bytes);
        alloc->kind             = OCM_LOCAL_GPU;
        alloc->u.local.bytes    = msg.u.alloc.bytes;
        cudaMalloc(alloc->u.local.ptr, msg.u.alloc.bytes);
        if (!alloc->u.local.ptr)
            goto out;
    }
    
    #endif
    #ifdef INFINIBAND
    else if (msg.u.alloc.type == ALLOC_MEM_RDMA) {
        printd("ALLOC_MEM_RDMA %lu bytes\n", msg.u.alloc.bytes);
        struct ib_params p;
        p.addr      = strdup(msg.u.alloc.u.rdma.ib_ip);
        p.port      = msg.u.alloc.u.rdma.port;
        p.buf_len   = (1 << 10); /* XXX accept func param for this */
        p.buf       = malloc(p.buf_len);
        if (!p.buf)
            goto out;

        printd("RDMA: local buf %lu bytes <-->"
                " server %s:%d (rank%d) buf %lu bytes\n",
                p.buf_len, p.addr, p.port,
                msg.u.alloc.remote_rank, msg.u.alloc.bytes);

        alloc->u.rdma.ib = ib_new(&p);
        if (!alloc->u.rdma.ib)
            goto out;

        INIT_LIST_HEAD(&alloc->link);
        alloc->kind                 = OCM_REMOTE_RDMA;
        alloc->u.rdma.remote_rank   = msg.u.alloc.remote_rank;
        alloc->u.rdma.remote_bytes  = msg.u.alloc.bytes;
        alloc->u.rdma.local_bytes   = p.buf_len;
        alloc->u.rdma.local_ptr     = p.buf;

        if (ib_connect(alloc->u.rdma.ib, false))
            goto out;

        printd("adding new alloc to list\n");
        lock_allocs();
        list_add(&alloc->link, &allocs);
        unlock_allocs();

        free(p.addr);
        p.addr = NULL;
    }
    #endif 

    #ifdef EXTOLL
    else if (msg.u.alloc.type == ALLOC_MEM_RMA) {
        printd("adding new lib_alloc to list\n");
        lock_allocs();
        list_add(&alloc->link, &allocs);
        unlock_allocs();

        BUG(1); /* TODO path not implemented... */
    }
    #endif

    else {
        BUG(1);
    }

    ret = 0;

out:
    if (ret) {
        if (alloc)
            free(alloc);
        alloc = NULL;
    }
    return alloc;
}

int
ocm_free(ocm_alloc_t a)
{
    ABORT(); /* XXX Code the protocol. */
}

int
ocm_localbuf(ocm_alloc_t a, void **buf, size_t *len)
{
    if (!a) return -1;
    if (a->kind == OCM_LOCAL_HOST) {
        *buf = a->u.local.ptr;
        *len = a->u.local.bytes;
    }
    #ifdef CUDA
    if (a->kind == OCM_LOCAL_GPU) {
        *buf = a->u.local.ptr;
        *len = a->u.local.bytes;
    }
    #endif
    #ifdef INFINIBAND
    else if (a->kind == OCM_REMOTE_RDMA) {
        *buf = a->u.rdma.local_ptr;
        *len = a->u.rdma.local_bytes;
    }
    #endif
    #ifdef EXTOLL
    else if (a->kind == OCM_REMOTE_RMA) {
        BUG(1);
    } 
    #endif
    else {
        BUG(1);
    }
    return 0;
}

bool
ocm_is_remote(ocm_alloc_t a)
{
  bool is_remote = true;

  if((a->kind == OCM_LOCAL_HOST) || (a->kind != OCM_LOCAL_GPU))
    is_remote = false;

    return is_remote;
}

int
ocm_remote_sz(ocm_alloc_t a, size_t *len)
{
    if (!a) return -1;
    if ((a->kind == OCM_LOCAL_HOST) || (a->kind != OCM_LOCAL_GPU))
    {
        return -1; /* there exists no remote buffer */
    }
    #ifdef INFINIBAND
    else if (a->kind == OCM_REMOTE_RDMA) {
        *len = a->u.rdma.remote_bytes;
    } 
    #endif
    #ifdef EXTOLL
    else if (a->kind == OCM_REMOTE_RMA) {
        BUG(1);
    }
    #endif
    else {
        BUG(1);
    }
    return 0;
}

int ocm_copy_out(void *dst, ocm_alloc_t src)
{
    return -1;
}

int ocm_copy_in(ocm_alloc_t dst, void *src)
{
    return -1;
}

int
ocm_copy(ocm_alloc_t dst, ocm_alloc_t src)
{
    #ifdef INFINIBAND
    /* from local */
    if (src->kind == OCM_LOCAL_RDMA)
    {
        /* local to local */
        if (dst->kind == OCM_LOCAL_HOST)
        {
            /* simple copy */
        }
        /* local to remote */
        else if (dst-> kind == OCM_REMOTE_RDMA)
        {
            ib_write(src->u.rdma.ib, 0, src->u.rdma.local_bytes);
        }
    }
    /* from remote */
    else if (src->kind == OCM_REMOTE_RDMA)
    {
        /* remote to local */
        if (dst->kind == OCM_LOCAL_RDMA)
        {
            ib_read(src->u.rdma.ib, 0, src->u.rdma.local_bytes);
        }
        /*// remote to remote 
 *         else if (dst->kind == OCM_REMOTE)
 *                 {
 *                         
 *                                 }*/
    }
    return 0;
    #endif

    return -1;
}


int
ocm_copy2(ocm_alloc_t src, int read)
{
    #ifdef INFINIBAND
    if (!read)
    {
        if(ib_write(src->u.rdma.ib, 0, src->u.rdma.local_bytes))
        {
            printf("write failed\n");
            return -1;
        }
    }
    else
    {
        if(ib_read(src->u.rdma.ib, 0, src->u.rdma.local_bytes))
        {
            printf("read failed\n");
            return -1;
        }
    }
    return 0;
    #endif

    return -1;
}

