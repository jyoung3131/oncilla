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
#include <msg.h>
#include <util/queue.h>

/* Defines */

/* Types */

/* Global state (externs) */

/* Static inline functions */

/* Function prototypes */

int mem_init(const char *nodefile_path);
int mem_new_request(struct message *m);
void mem_fin(void);
void mem_set_outbox(struct queue *outbox);
int mem_get_rank(void);

#endif  /* __MEM__ */
