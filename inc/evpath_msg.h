/**
 * file: evpath_msg.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: definitions for network messages
 *
 * EVPath doesn't like unions, so .. avoid them.
 */

#ifndef __EVPATH_MSG__
#define __EVPATH_MSG__

/* System includes */
#include <evpath.h>

/* Other project includes */

/* Project includes */

/* Defines */

/* Types */

struct evmsg
{
    int rank;
};

typedef struct evmsg evmsg, *evmsg_ptr;

/* Global state */

extern FMField evfmt_evmsg[];
extern FMStructDescRec fmt_list[];

/* Function prototypes */

int ev_init(int rank);

#endif  /* __EVPATH_MSG__ */
