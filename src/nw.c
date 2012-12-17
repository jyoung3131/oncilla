/**
 * file: nw.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: network messaging state and initialization
 */

/* System includes */
#include <mpi.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

/* Other project includes */

/* Project includes */
#include <io/nw.h>
#include <util/queue.h>
#include <debug.h>

/* Directory includes */
#include "msg.h"

/* Globals */

/* Internal definitions */

struct nw_message
{
    int dest_rank;
    struct message m; /* payload */
};

struct worker
{
    pthread_t tid;
    volatile bool alive; /* gcc seems to not reread this when updated */

    int mpi_procs, mpi_rank;
    MPI_Request request; /* handle for outstanding recv requests */

    struct message_forward forward; /* for us to send messages out */
    struct queue out_q; /* queue of nw_message */
};

/* Internal state */

static struct worker worker; /* only one thread allowed due to OpenMPI */

/* Private functions */

static inline bool
probe_waiting(bool *has_msg)
{
    int flag, retval;
    retval = (MPI_SUCCESS == MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG,
                MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE));
    *has_msg = !!flag; /* MPI spec says "flag=true" means msg waiting */
    return retval;
}

static inline bool
do_recv(struct message *m)
{
    printd("receiving MPI message\n");
    return MPI_SUCCESS == MPI_Recv(m, sizeof(*m), MPI_BYTE, MPI_ANY_SOURCE,
            MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

static int
post_send(struct message *m, int rank)
{
    /* TODO in reality we should keep track of these to make sure all send's are
     * pushed out correctly, but if one MPI rank fails, the entire thing is shut
     * down, so... we assume the network doesn't crap on us */
    MPI_Request req;
    int err;

    printd("sending MPI message to rank %d\n", rank);

    /* if a request, go to rank 0, else back to origin */
    err = MPI_Isend(m, sizeof(*m), MPI_BYTE, rank, 0, MPI_COMM_WORLD, &req); 
    return (err == MPI_SUCCESS ? 0 : -1);
}

/* another module gives us the forward, we use that to pass on recvd messages */
static int
export_msg(struct message *m)
{
    int retval = 0;
    struct message_forward *f = &worker.forward;
    printd("popping msg\n");
    if (f->handle)
        retval = f->handle(f->q, m);
    else
        retval = -1;
    return retval;
}

/* other modules submit messages to us via this function */
static int
import_msg(struct queue *ignored, struct message *m, ...)
{
    va_list extra;
    struct nw_message nw_m = { .dest_rank = -1, .m = *m };

    if (!m) return -1;

    /* extract third argument as int - the rank */
    va_start(extra, m);
    nw_m.dest_rank = va_arg(extra, int);
    va_end(extra);

    printd("appending msg, for rank %d\n", nw_m.dest_rank);
    q_push(&worker.out_q, &nw_m);
    return 0;
}

static void
listener_cleanup(void *arg)
{
    worker.alive = false;
    MPI_Finalize();
}

/* accepts new connections, ONLY this thread will perform MPI because OpenMPI
 * wasn't compiled to support more than one thread... */
static void *
worker_thread(void *arg)
{
    int pstate;
    bool has_msg;
    struct nw_message nw_msg;
    struct message msg;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &worker.mpi_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &worker.mpi_rank);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &pstate);
    pthread_cleanup_push(listener_cleanup, NULL);

    printd("IO thread alive on rank %d\n", worker.mpi_rank);

    worker.alive = true;

    while (true) /* main loop */
    {
        /* empty the work queue */
        while (!q_empty(&worker.out_q))
        {
            if (q_pop(&worker.out_q, &nw_msg) != 0)
                ABORT();

            if (post_send(&nw_msg.m, nw_msg.dest_rank) < 0)
                ABORT();
        }

        /* Check for incoming messages, export to other module.
         * Note: probe doesn't actually pull in the message. To pull in the
         * message, post an Irecv and use MPI_Test instead of Iprobe */
        if (!probe_waiting(&has_msg))
            ABORT();
        if (has_msg)
        {
            printd("receiving a message\n");
            if (!do_recv(&msg))
                ABORT();
            if (export_msg(&msg) < 0)
                ABORT();
        }

        usleep(500);
    }

    pthread_cleanup_pop(1);
    return NULL;
}

/* Public functions */

int
nw_init(void)
{
    printd("IO interface initializing\n");
    memset(&worker, 0, sizeof(worker));
    q_init(&worker.out_q, sizeof(struct nw_message));
    return 0;
}

/* set the 'forward' this module should call when exporting a msg */
int
nw_set_export(struct message_forward *f)
{
    if (!f) return -1;
    worker.forward.handle = f->handle;
    worker.forward.q = f->q;
    return 0;
}

/* obtain nw func to call when passing msg to this module */
struct message_forward
nw_get_import(void)
{
    struct message_forward f = { .handle = import_msg, .q = NULL };
    return f;
}

int
nw_launch(void)
{
    int err;
    if (worker.alive) return -1;
    printd("IO interface launching worker thread\n");
    err = pthread_create(&worker.tid, NULL, worker_thread, NULL);
    if (err < 0) return -1;
    while (!worker.alive) ;
    return 0;
}

int
nw_get_rank(void)
{
    return worker.mpi_rank;
}

void
nw_fin(void)
{
    if (!worker.alive) return;
    printd("IO interface finalizing\n");
    if (0 == pthread_cancel(worker.tid))
        pthread_join(worker.tid, NULL);
    while (worker.alive) ;
    q_free(&worker.out_q);
}
