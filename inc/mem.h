/**
 * file: mem.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __MEM__
#define __MEM__

/* System includes */
#include <stdio.h>

/* Other project includes */

/* Project includes */
#include <util/queue.h>

/* Defines */

/* Types */

/* Global state (externs) */

/* Static inline functions */

/* Function prototypes */

int mem_init(void);
int mem_launch(void);
int mem_add_msg(struct message *m);
void mem_fin(void);
void mem_set_outbox(struct queue *outbox);

#endif  /* __MEM__ */
