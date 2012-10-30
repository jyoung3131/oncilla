/**
 * file: alloc.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: TODO
 */

#ifndef __ALLOC__
#define __ALLOC__

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
int mem_new_request(struct message *m);
int mem_set_export(struct message_forward *f);
struct message_forward mem_get_import(void);
void mem_fin(void);

#endif  /* __ALLOC__ */
