/**
 * file: mpi_msg.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: MPI messaging state and initialization
 */

/* System includes */
#include <mpi.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */

/* Project includes */
#include <util/list.h>
#include <mpi_msg.h>

/* Globals */

/* Internal state */

static int mpi_procs = 0, mpi_rank = -1;

static bool listener_alive;
static pthread_t listener_id;

/* Private functions */

static void
process_msg(struct mpi_message *msg)
{
}

static void
listener_cleanup(void *ignored)
{
    listener_alive = false;
    printf(">> listener thread exiting\n");
}

static void *
listener(void *ignored)
{
    int pstate, mpi_err;
    struct mpi_message mpi_msg;
    MPI_Status mpi_status;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &pstate);
    pthread_cleanup_push(listener_cleanup, NULL);

    listener_alive = true;

    while (true)
    {
        mpi_err = MPI_Recv(&mpi_msg, sizeof(mpi_msg), MPI_BYTE,
                MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
        if (MPI_SUCCESS == mpi_err)
        {
            printf(">> recv'd msg from rank%d\n", mpi_msg.rank);
        }
        else
            fprintf(stderr, "MPI_Recv failed\n");
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

static void
spawn_listener(void)
{
    pthread_create(&listener_id, NULL, listener, NULL);
    while (!listener_alive) ;
}

static void
stop_listener(void)
{
    if (0 == pthread_cancel(listener_id))
        pthread_join(listener_id, NULL);
}

/* Public functions */

void mpi_init(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    spawn_listener();
    MPI_Barrier(MPI_COMM_WORLD);
    printf(">> barrier\n");

    if (mpi_rank ==  1)
    {
        struct mpi_message msg;
        msg.rank = mpi_rank;
        if (MPI_SUCCESS !=
                MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD))
            fprintf(stderr, ">> error sending to rank0\n");
        if (MPI_SUCCESS !=
                MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD))
            fprintf(stderr, ">> error sending to rank0\n");
        if (MPI_SUCCESS !=
                MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD))
            fprintf(stderr, ">> error sending to rank0\n");
        if (MPI_SUCCESS !=
                MPI_Send(&msg, sizeof(msg), MPI_BYTE, 0, 0, MPI_COMM_WORLD))
            fprintf(stderr, ">> error sending to rank0\n");
    }
}

void mpi_fin(void)
{
    stop_listener();
    while (listener_alive) ;

    MPI_Finalize();
}
