/**
 * file: main.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: daemon process
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */
#include <evpath.h>

/* Project includes */
#include <mpi_msg.h>

/* Globals */

/* Functions */

/* TODO Have a MQ recv messages and convert them to MPI messages */

int main(int argc, char *argv[])
{
    mpi_init(argc, argv);
    printf(">> main, sleeping 4s\n");
    sleep(4);
    printf(">> main, done sleeping\n");
    mpi_fin();
    return 0;
}
