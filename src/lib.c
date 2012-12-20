/**
 * file: lib.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: library apps link with; libocm.so and oncillamem.h
 */

/* System includes */
#include <stdio.h>

/* Other project includes */

/* Project includes */
#include <oncillamem.h>
#include <mq.h>
#include <debug.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

struct allocation
{
    struct list_head link;
    void *ptr;
};
#define for_each_alloc(alloc, allocs) \
    list_for_each_entry(alloc, &allocs, link)
#define lock_allocs()   pthread_mutex_lock(&allocs_lock)
#define unlock_allocs() pthread_mutex_unlock(&allocs_lock)

/* requests initiated by app */
struct request
{
#error fuckfuckfuck
};

/* Internal state */

static LIST_HEAD(allocs);
static pthread_mutex_t allocs_lock = PTHREAD_MUTEX_INITIALIZER;

/* Private functions */

static void
process_daemon_msg(msg_event e, pid_t notused, void *data)
{
}

/* Global functions */

int
ocm_init(void)
{
    if (0 > attach_init())
        return -1;
    if (0 > attach_send_connect()) {
        attach_tini();
        return -1;
    }
    printd("Attached to daemon\n");
    return 0;
}

int
ocm_tini(void)
{
    if (0> attach_send_disconnect())
        return -1;
    if (0 > attach_tini())
        return -1;
    printd("Detached from daemon\n");
    return 0;
}

ocm_alloc_t
ocm_alloc(size_t bytes)
{
    /* XXX Code the protocol. */

    struct allocation a;
    INIT_LIST_HEAD(&a.link);

    struct message msg;
    msg.type = MSG_REQ_ALLOC;
    msg.status = MSG_REQUEST;
    msg.pid = getpid();
    /* u.req.orig_rank filled out by daemon */
    msg.u.req.bytes = bytes;
    
    if (0 > attach_send(&msg)) {
        printd("error sending msg\n");
        return NULL;
    }

    /* XXX alloc should create a new request in a list, then block on a
     * condition variable. daemon will see request, then should be able to send
     * messages back to us (lib) to carry out certain functions, e.g. call
     * malloc, call something in libRMA. Then some 'release' message can unlock
     * the condition variable allowing this function to continue.
     */

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
