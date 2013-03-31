/**
 * file: msg.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __MSG_H__
#define __MSG_H__

/* System includes */
#include <stdio.h>
#include <limits.h>

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

    MSG_ADD_NODE, /* ranks > 0 reporting to rank 0 on bootup */

    MSG_REQ_ALLOC, /* alloc_request msg; resp is alloc_ation msg */
    MSG_DO_ALLOC, /* alloc_do message */

    MSG_REQ_FREE, /* lib requests free of mem */
    MSG_DO_FREE, /* mem module asks region be free'd */

    MSG_RELEASE_APP, /* release app thread, req has completed */

    MSG_ANY, /* flag indicating any of the above */
    MSG_MAX /* utilize enum as int when allocating arrays of msg types */
};

enum message_status
{
    MSG_NO_STATUS = 0, /* used when status is irrelevant */
    MSG_REQUEST,
    MSG_RESPONSE
};

///The message structure is passed between Oncilla daemon
///processes and contains information about the allocation
///request (and response) and the allocation
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
        struct {
            struct alloc_node_config config;
        } node;
    } u;
};

/* Global state (externs) */

/* Static inline functions */

static inline const char *
MSG_TYPE2STR(enum message_type t)
{
    switch (t) {
    case MSG_INVALID:           return "MSG_INVALID";
    case MSG_CONNECT:           return "MSG_CONNECT";
    case MSG_CONNECT_CONFIRM:   return "MSG_CONNECT_CONFIRM";
    case MSG_DISCONNECT:        return "MSG_DISCONNECT";
    case MSG_REQ_ALLOC:         return "MSG_REQ_ALLOC";
    case MSG_DO_ALLOC:          return "MSG_DO_ALLOC";
    case MSG_ADD_NODE:          return "MSG_ADD_NODE";
    case MSG_REQ_FREE:          return "MSG_REQ_FREE";
    case MSG_DO_FREE:           return "MSG_DO_FREE";
    case MSG_RELEASE_APP:       return "MSG_RELEASE_APP";
    case MSG_ANY:               return "MSG_ANY";
    case MSG_MAX:               return "MSG_MAX";
    default:                    return "INVALID MSG TYPE";
    }
}

static inline const char *
MSG_STATUS2STR(enum message_status s)
{
    switch (s) {
    case MSG_NO_STATUS: return "MSG_NO_STATUS";
    case MSG_REQUEST:   return "MSG_REQUEST";
    case MSG_RESPONSE:  return "MSG_RESPONSE";
    default:            return "INVALID MSG STATUS";
    }
}

/* Function prototypes */

#endif  /* __MSG_H__ */
