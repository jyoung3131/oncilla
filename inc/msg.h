/**
 * file: msg.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __MSG__
#define __MSG__

/* System includes */
#include <stdio.h>

/* Other project includes */

/* Project includes */
#include <util/queue.h>
#include <alloc.h>

/* Defines */

/* Types */

enum message_type
{
    MSG_INVALID = 0,
    MSG_REQ_ALLOC, /* request for new memory, handled by rank 0 */
    MSG_DO_ALLOC, /* ask a node to allocate memory */
    MSG_DO_FREE, /* ask a node to free memory */
    MSG_ANY, /* flag indicating any of the below */
    MSG_MAX /* utilize enum as int when allocating arrays of msg types */
};

enum message_status
{
    MSG_REQUEST, /* send to rank 0 */
    MSG_RESPONSE, /* return back to sender */
    MSG_EOL /* return to application */
};

struct message
{
    enum message_type type;
    enum message_status status;

    pid_t pid; /* app which made request */
    int rank; /* rank which made request */

    /* message specifics */
    union {
        struct alloc_request req;
        struct alloc_ation alloc;
    } u;
};

typedef int (*msg_forward)(struct queue *q, struct message *m, ...);

struct message_forward
{
    msg_forward handle;
    struct queue *q;
};

/* Global state (externs) */

/* Static inline functions */

static inline const char *
MSG_TYPE2CHAR(enum message_type type)
{
    switch (type)
    {
        case MSG_INVALID: return "MSG_INVALID";
        case MSG_REQ_ALLOC: return "MSG_REQ_ALLOC";
        case MSG_DO_ALLOC: return "MSG_DO_ALLOC";
        case MSG_DO_FREE: return "MSG_DO_FREE";
        case MSG_ANY: return "MSG_ANY";
        case MSG_MAX: return "MSG_MAX";
        default: return "Unknown message_type";
    }
}

static inline const char *
MSG_STATUS2CHAR(enum message_status status)
{
    switch (status)
    {
        case MSG_REQUEST: return "MSG_REQUEST";
        case MSG_RESPONSE: return "MSG_RESPONSE";
        default: return "Unknown message_status";
    }
}

static inline bool
MSG_TYPE_IS_VALID(enum message_type type)
{
    return ((type > MSG_INVALID) && (type < MSG_MAX));
}

static inline bool
MSG_IS_VALID(struct message *m)
{
    return MSG_TYPE_IS_VALID(m->type);
}

/* Function prototypes */

#endif  /* __MSG__ */
