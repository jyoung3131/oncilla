/**
 * file: nw.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: distributed state definitions
 */

#ifndef __NW__
#define __NW__

/* System includes */
#include <pthread.h>
#include <mpi.h>

/* Other project includes */

/* Project includes */
#include <msg.h>

/* Defines */

#define NW_LISTEN_PORT      50000
#define NW_LISTEN_PORT_STR  "50000"

/* Types */

/* Global state (externs) */

/* Function prototypes */

int nw_init(void);
int nw_launch(void);
int nw_get_rank(void);
void nw_fin(void);
int nw_send(struct message *m, int to_rank);
int nw_set_recv_q(struct queue *q);

#endif  /* __NW__ */
