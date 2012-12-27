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

#define for_each_alloc(alloc, allocs) \
    list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()   pthread_mutex_lock(&allocs_lock)
#define unlock_allocs() pthread_mutex_unlock(&allocs_lock)

/* Internal state */

static LIST_HEAD(allocs); /* list of struct alloc_ation */
//static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;

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

    /* tell daemon who we are */
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
ocm_alloc(size_t bytes)
{
    //ocm_alloc_t ocm_alloc = NULL;

    /* XXX Code the protocol. */

    //struct allocation a;
    //INIT_LIST_HEAD(&a.link);

    struct message msg;
    msg.type = MSG_REQ_ALLOC;
    msg.status = MSG_REQUEST;
    msg.pid = getpid();
    /* u.req.orig_rank filled out by daemon */
    msg.u.req.bytes = bytes;
    
    printd("msg sent to daemon\n");
    if (0 > pmsg_send(PMSG_DAEMON_PID, &msg))
        return NULL;

    printd("waiting for reply from daemon\n");
    if (0 > pmsg_recv(&msg, true))
        return NULL;
    printd("got a reply\n");
    while (msg.type == MSG_DO_ALLOC) {
        struct alloc_ation *a = &msg.u.alloc;
        msg.status = MSG_RESPONSE;
        if (ALLOC_MEM_HOST == a->type) {
            printd("got alloc mem msg, calling malloc\n");
            a->ptr = (uintptr_t)malloc(a->bytes);
            ABORT2(!a->ptr);
            if (0 > pmsg_send(PMSG_DAEMON_PID, &msg))
                return NULL;
        } else {
            BUG(1);
        }
        printd("waiting for next message from daemon...\n");
        if (0 > pmsg_recv(&msg, true))
            return NULL;
    }

    BUG(msg.type != MSG_RELEASE_APP);
    printd("got release msg from daemon\n");

    /* TODO append alloc_ation struct to list and return */

    return NULL; /* XXX */
}

void
ocm_free(ocm_alloc_t a)
{
    ABORT(); /* XXX Code the protocol. */
}

int
ocm_copy(ocm_alloc_t dst, ocm_alloc_t src)
{
    return -1;
}
