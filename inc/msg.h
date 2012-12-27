/**
 * file: msg.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __MSG_H__
#define __MSG_H__

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

    MSG_CONNECT, /* app -> daemon */
    MSG_CONNECT_CONFIRM, /* daemon -> app */

    MSG_DISCONNECT, /* app -> daemon */

    MSG_REQ_ALLOC, /* alloc_request msg; resp is alloc_ation msg */
    MSG_DO_ALLOC, /* alloc_do message */
    MSG_FIN_ALLOC, /* alloc completed on all participating nodes */

    MSG_REQ_FREE, /* lib requests free of mem */
    MSG_DO_FREE, /* mem module asks region be free'd */

    MSG_RELEASE_APP, /* release app thread, req has completed */

    MSG_ANY, /* flag indicating any of the above */
    MSG_MAX /* utilize enum as int when allocating arrays of msg types */
};

enum message_status
{
    MSG_NO_STATUS = 0,
    MSG_REQUEST,
    MSG_RESPONSE
};

struct message
{
    enum message_type type;
    enum message_status status;

    pid_t pid; /* app which made request */
    int rank; /* rank (app) which made request */

    /* message specifics */
    union {
        struct alloc_request req;
        struct alloc_ation alloc;
        struct alloc_do d;
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

#endif  /* __MSG_H__ */
