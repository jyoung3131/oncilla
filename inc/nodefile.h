/**
 * file: nodefile.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: node information
 */

#ifndef __NODEFILE_H__
#define __NODEFILE_H__

/* System includes */

/* Other project includes */
#include <alloc.h>

/* Project includes */

/* Defines */

struct node_entry
{
    char dns[HOST_NAME_MAX];
    char ip_eth[HOST_NAME_MAX];
    int  ocm_port;
    int  rdmacm_port;
    /* runtime info, only rank0 has these initialized */
    struct alloc_node_config *config;
};

/* Types */

/* Global state (externs) */

extern struct node_entry *node_file;
extern int node_file_entries;

/* Static inline functions */

/* Function prototypes */
int parse_nodefile(const char *path, int *_myrank /* out */);

#endif  /* __NODEFILE_H__ */
