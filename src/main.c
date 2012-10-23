/**
 * file: main.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: daemon process
 */

/* System includes */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */
#include <evpath.h>

/* Project includes */
#include <evpath_msg.h>

/* Globals */

/* XXX Should be put into some MPI file */
static int mpi_procs = 0, mpi_rank = -1;

/* Functions */

/*
 * make a message queue handler here which converts a MQ message into an evpath
 * request of some sort
 */

int main(int argc, char *argv[])
{
    printf("Configuring MPI\n");

    /* MPI configuration */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    printf("pid %d is rank %d\n", getpid(), mpi_rank);

    ev_init(mpi_rank);

    /* MPI teardown */

    MPI_Finalize();

    return 0;
}
