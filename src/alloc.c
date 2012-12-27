/**
 * file: alloc.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: memory allocation governer
 */

/* System includes */
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <alloc.h>
#include <debug.h>
#include <util/list.h>

/* Directory includes */

/* Globals */

/* Internal definitions */

struct node
{
    struct list_head link;
    int rank;
    struct alloc_node_config c; /* static hardware */
    struct alloc_node_config c_avail; /* available hardware */
};

/* Internal state */

/* TODO these may need locking (allocations will reduce mem availability) */
/* lists maintained by rank 0 (master node) only */
static LIST_HEAD(nodes);
static LIST_HEAD(allocs); /* list of alloc_ation's */

/* Private functions */

/* Public functions */

int
alloc_add_node(int rank, struct alloc_node_config *c)
{
    struct node *n;

    if (!c) return -1;

    n = calloc(1, sizeof(*n));
    if (!n) ABORT();

    INIT_LIST_HEAD(&n->link);
    n->rank = rank;
    n->c = *c;

    list_add(&n->link, &nodes);

    printd("new node added: ram=%lu gpus=%d\n", c->ram, c->num_gpu);
    return 0;
}

/* TODO remove node? MPI v3 is what supports dynamic process joining/removing
 * the communication set... otherwise we assume all ranks/nodes that are up are
 * all immediately available and don't go away */

/* consults the metadata to locate (and reserve) memories. only rank 0 */
int
alloc_find(struct alloc_request *r, struct alloc_ation *a)
{
    if (!r || !a) return -1;

    /* TODO Use the request and list of nodes, etc to figure out where/how to
     * satisfy the memory request. */

    /* XXX hard-code local allocation for now (ie same as reg malloc) */
    a->rank = r->orig_rank;
    a->type = ALLOC_MEM_HOST;
    a->bytes = r->bytes;

    printd("called; local alloc hard-coded\n");

    return 0;
}

/* on this node, carry out an allocation previously made by alloc_find */
/* XXX
 * This function should only carry out requests for allocation that necessitate
 * involvement of a remote node. Local allocations must be passed back to the
 * application for the library to make
 */
int
alloc_ate(struct alloc_ation *a)
{
    if (!a) return -1;

    printd("called\n");

    /* TODO iterate through items in the allocation and make calls into the
     * associated resource - gpu, call CUDA; host, call malloc and/or libRMA */

    return 0;
}
