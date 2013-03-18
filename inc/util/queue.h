/**
 * file: queues.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __QUEUES__
#define __QUEUES__

/* System includes */
#include <stdbool.h>

/* Other project includes */

/* Project includes */
#include <util/list.h>

/* Defines */

#define q_empty(q)  ((q)->num == 0)

/* Types */

struct qentry
{
    struct list_head link;
    void *data;
};

struct queue
{
    struct list_head work, free; /* queued and free entries */
    pthread_mutex_t lock;
    unsigned int num, size; /* num queued and total entries */
    size_t data_size;
};

/* Global state (externs) */

/* Function prototypes */

void q_init2(struct queue *q, size_t elems, size_t data_size);
void q_init(struct queue *q, size_t data_size);
void q_free(struct queue *q);
void q_push(struct queue *q, void *data);
int q_pop(struct queue *q, void *data);

#endif  /* __QUEUES__ */
