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

/* Internal state */

static LIST_HEAD(allocs);

static struct mq_state recv_mq, send_mq;

/* Private functions */

/* Global functions */

int
ocm_init(void)
{
    if (0 > attach_init(&recv_mq, &send_mq))
        return -1;
    if (0 > attach_send_connect(&recv_mq, &send_mq)) {
        attach_tini(&recv_mq, &send_mq);
        return -1;
    }
    printd("Attached to daemon\n");
    return 0;
}

int ocm_tini(void)
{
    if (0> attach_send_disconnect(&recv_mq, &send_mq))
        return -1;
    if (0 > attach_tini(&recv_mq, &send_mq))
        return -1;
    printd("Detached from daemon\n");
    return 0;
}

ocm_alloc_t
ocm_alloc(size_t bytes)
{
    return NULL;
}

void
ocm_free(ocm_alloc_t a)
{
}

int
ocm_copy(ocm_alloc_t dst, ocm_alloc_t src)
{
    return -1;
}
