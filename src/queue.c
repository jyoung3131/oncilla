/**
 * file: queue.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

/* System includes */
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <debug.h>

/* Other project includes */

/* Project includes */
#include <util/queue.h>

/* Globals */

/* Internal definitions */

#define QUEUES_ALLOC_MORE_AMT   32

/* Internal state */

/* Private functions */

static void
__init_qentry(struct qentry *e)
{
    INIT_LIST_HEAD(&e->link);
    e->data = NULL;
}

static void
__init_qentry2(struct qentry *e, void *data)
{
    INIT_LIST_HEAD(&e->link);
    e->data = data;
}

static void
__alloc_more(struct queue *q, size_t num, size_t data_size)
{
    struct qentry *e;
    void *data;
    q->size += num;
    while (num-- > 0)
    {
        e = calloc(1, sizeof(*e));
        if (!e) ABORT();
        data = malloc(data_size);
        if (!data) ABORT();
        __init_qentry2(e, data);
        list_add(&e->link, &q->free);
    }
}

/* take entry from freelist, initialize, and push onto tail of main queue */
static void
__push(struct queue *q, void *data)
{
    struct qentry *e;
    if (list_empty(&q->free))
        __alloc_more(q, QUEUES_ALLOC_MORE_AMT, q->data_size);
    q->num++;
    e = list_first_entry(&q->free, struct qentry, link);
    memcpy(e->data, data, q->data_size);
    list_move_tail(&e->link, &q->work);
}

/* pop from head of main queue, extract, and push to freelist */
static void
__pop(struct queue *q, void *data)
{
    struct qentry *e;
    if (list_empty(&q->work))
        return;
    q->num--;
    e = list_first_entry(&q->work, struct qentry, link);
    memcpy(data, e->data, q->data_size);
    list_del(&e->link);
    __init_qentry2(e, data);
    list_add(&e->link, &q->free);
}

/* Public functions */

void q_init2(struct queue *q, size_t elems, size_t data_size)
{
    if (!q || elems == 0) return;
    if (!q_empty(q)) return;

    INIT_LIST_HEAD(&q->work);
    INIT_LIST_HEAD(&q->free);
    pthread_mutex_init(&q->lock, NULL);
    q->num = 0;
    q->size = 0;
    q->data_size = data_size;

    __alloc_more(q, elems, data_size);
}

void q_init(struct queue *q, size_t data_size)
{
    q_init2(q, QUEUES_ALLOC_MORE_AMT, data_size);
}

void q_free(struct queue *q)
{
    struct qentry *e, *tmp;
    if (!q) return;
    list_for_each_entry_safe(e, tmp, &q->work, link)
    {
        list_del(&e->link);
        free(e);
    }
    list_for_each_entry_safe(e, tmp, &q->free, link)
    {
        list_del(&e->link);
        free(e);
    }
    memset(q, 0, sizeof(*q));
}

void q_push(struct queue *q, void *data)
{
    if (!q || !data) return;
    pthread_mutex_lock(&q->lock);
    __push(q, data);
    pthread_mutex_unlock(&q->lock);
}

int q_pop(struct queue *q, void *data)
{
    int retval = 0;
    if (!q || !data) return -1;
    pthread_mutex_lock(&q->lock);
    if (q_empty(q))
        retval = -1;
    else
        __pop(q, data);
    pthread_mutex_unlock(&q->lock);
    return retval;
}

/* TODO provide public q_lock/unlock functions to allow push and pop to proceed
 * without locking overhead on large queues */
