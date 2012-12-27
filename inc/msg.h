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

/* Global state (externs) */

/* Static inline functions */

/* Function prototypes */

#endif  /* __MSG_H__ */
