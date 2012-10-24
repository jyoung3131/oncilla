/**
 * file: evpath_msg.h
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: definitions for network messages
 */

#ifndef __MPI_MSG__
#define __MPI_MSG__

/* System includes */

/* Other project includes */

/* Project includes */

/* Defines */

/* Types */

enum mpi_message_type
{
    MPI_MSG_INVALID = 0
};

struct mpi_message
{
    int rank;
    /* TODO place some other struct here. MPI just carries it */
};

/* Global state (externs) */

/* Function prototypes */

void mpi_init(int argc, char *argv[]);
void mpi_fin(void);

#endif  /* __MPI_MSG__ */
