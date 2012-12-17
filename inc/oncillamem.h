/**
 * file: oncillamem.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: app library interface for OCM
 */

#ifndef __ONCILLAMEM_H__
#define __ONCILLAMEM_H__

/* System includes */
#include <stdlib.h>

/* Other project includes */

/* Project includes */
#include <util/list.h>

/* Directory includes */

/* Definitions */

typedef void * ocm_alloc_t;

/* Globals */

/* Private functions */

/* Global functions */

int ocm_init(void);
int ocm_tini(void);
ocm_alloc_t ocm_alloc(size_t bytes);
void ocm_free(ocm_alloc_t a);
int ocm_copy(ocm_alloc_t dst, ocm_alloc_t src);

#endif  /* __ONCILLAMEM_H__ */
