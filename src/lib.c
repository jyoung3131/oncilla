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
    int err;

    /* open resources */
    if (pmsg_init(sizeof(struct message)) < 0) {
        printd("error pmsg_init\n");
        return -1;
    }
    if (pmsg_open(getpid()) < 0) {
        printd("error pmsg_open\n");
        return -1;
    }
    while (tries-- > 0) {
        err = pmsg_attach(PMSG_DAEMON_PID);
        if (err == 0)
            break;
        usleep(10000);
    }
    if (tries <= 0) {
        printd("cannot open daemon mailbox\n");
        pmsg_close();
        return -1;
    }

    #ifdef INFINIBAND
    if (ib_init()) {
        printd("ib failed to initialize\n");
        return -1;
    }
    #endif

    /* tell daemon who we are, wait for confirmation msg */
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_CONNECT;
    msg.pid = getpid();
    if (pmsg_send(PMSG_DAEMON_PID, &msg) < 0) {
        printd("error sending msg to daemon\n");
        return -1;
    }
    if (pmsg_recv(&msg, true) < 0) {
        printd("error receiving connect confirm from daemon\n");
        return -1;
    }
    if (msg.type != MSG_CONNECT_CONFIRM) {
        printf("daemon denied attaching: msg %d\n", msg.type);
        return -1;
    }
    
    printd("attached to daemon\n");
    return 0;
}

int
ocm_tini(void)
{
    struct message msg;

    /* tell daemon we're leaving */
    msg.type = MSG_DISCONNECT;
    msg.pid = getpid();
    if (pmsg_send(PMSG_DAEMON_PID, &msg) < 0) {
        printd("error sending msg to daemon\n");
        return -1;
    }

    /* close resources */
    if (pmsg_detach(PMSG_DAEMON_PID) < 0) {
        printd("error detaching from daemon\n");
        return -1;
    }
    if (pmsg_close() < 0) {
        printd("error closing mailbox\n");
        return -1;
    }

    printd("detached from daemon\n");
    return 0;
}

ocm_alloc_t
ocm_alloc(size_t bytes, enum ocm_kind kind)
{
    struct message msg;
    struct lib_alloc *lib_alloc = calloc(1, sizeof(*lib_alloc));
    struct alloc_ation *msg_alloc = NULL;

    if (!lib_alloc) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    msg.type = MSG_REQ_ALLOC;
    msg.status = MSG_REQUEST;
    msg.pid = getpid();
    msg.u.req.bytes = bytes;

    if (kind == OCM_LOCAL)
        msg.u.req.type = ALLOC_MEM_HOST;
    else if (kind == OCM_REMOTE_RDMA)
        msg.u.req.type = ALLOC_MEM_RDMA;
    else if (kind == OCM_REMOTE_RMA)
        return NULL; /* TODO */
    else
        return NULL;
    
    printd("msg sent to daemon\n");
    if (pmsg_send(PMSG_DAEMON_PID, &msg))
        return NULL;

    printd("waiting for reply from daemon\n");
    if (pmsg_recv(&msg, true))
        return NULL;

    if (msg.type != MSG_RELEASE_APP)
        BUG(1);

    msg_alloc = &msg.u.alloc;
    msg.status = MSG_RESPONSE;

    if (ALLOC_MEM_HOST == msg_alloc->type) {
        printd("ALLOC_MEM_HOST %lu bytes\n", msg_alloc->bytes);
        lib_alloc->kind             = msg_alloc->type;
        lib_alloc->u.local.bytes    = msg_alloc->bytes;
        lib_alloc->u.local.ptr      = malloc(msg_alloc->bytes);
        ABORT2(!lib_alloc->u.local.ptr);
    }
    #ifdef INFINIBAND
    else if (ALLOC_MEM_RDMA == msg_alloc->type) {
        printd("ALLOC_MEM_RDMA %lu bytes\n", msg_alloc->bytes);
        struct ib_params p;
        p.addr      = strdup(msg_alloc->u.rdma.ib_ip);
        p.port      = msg_alloc->u.rdma.port;
        p.buf_len   = (1 << 10); /* XXX accept func param for this */
        p.buf       = malloc(p.buf_len);
        if (!p.buf)
            ABORT();

        printd("RDMA: local buf %lu bytes <-->"
                " server %s:%d (rank %d) buf %lu bytes\n",
                p.buf_len, p.addr, p.port, msg_alloc->remote_rank, msg_alloc->bytes);

        if (!(lib_alloc->u.rdma.ib = ib_new(&p))) {
            printd("error allocating new ib state\n");
            return NULL;
        }
        INIT_LIST_HEAD(&lib_alloc->link);
        lib_alloc->kind                 = msg_alloc->type;
        lib_alloc->u.rdma.remote_rank   = msg_alloc->remote_rank;
        lib_alloc->u.rdma.remote_bytes  = msg_alloc->bytes;
        lib_alloc->u.rdma.local_bytes   = p.buf_len;
        lib_alloc->u.rdma.local_ptr     = p.buf;

        if (ib_connect(lib_alloc->u.rdma.ib, false)) {
            printd("error connecting to server\n");
            return NULL;
        }

        printd("adding new lib_alloc to list\n");
        lock_allocs();
        list_add(&lib_alloc->link, &allocs);
        unlock_allocs();

        free(p.addr);
        p.addr = NULL;
    }
    #endif 

    #ifdef EXTOLL
    else if (ALLOC_MEM_RMA == msg_alloc->type) {
        printd("adding new lib_alloc to list\n");
        lock_allocs();
        list_add(&lib_alloc->link, &allocs);
        unlock_allocs();

        BUG(1); /* TODO path not implemented... */
    }
    #endif

    else {
        BUG(1); /* protocol error */
    }

    return lib_alloc;
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
    if (a->kind == OCM_LOCAL) {
        *buf = a->u.local.ptr;
        *len = a->u.local.bytes;
    }
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

int
ocm_remote_sz(ocm_alloc_t a, size_t *len)
{
    if (!a) return -1;
    if (a->kind == OCM_LOCAL) {
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
    return -1;
}
