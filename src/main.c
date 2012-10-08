/**
 * file: main.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: daemon process
 */

/* System includes */
#include <mpi.h>
#include <stdio.h>
#include <string.h>

/* Other project includes */
#include <evpath.h>

/* Project includes */
#include <evpath_msg.h>

static void *
recv_msg(void *data)
{
    return NULL;
}

int main(int argc, char *argv[])
{
    int mpi_procs = 0, mpi_rank = -1;

    /* MPI configuration */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

    /* EVPath configuration */

#if 0
    attr_list attrs = create_attr_list();
    conn_mgr = CManager_create();

    add_int_attr(attrs, attr_atom_from_string("IP_PORT"), 12345);

    CMlisten_specific(conn_mgr, attrs);

    stone = EValloc_stone(conn_mgr);

    ResourceNode *resourceNode;

    EVassoc_terminal_action(conn_mgr, stone, resourceNode->resource.format,
            receive_handler, &(resourceNode->resource));

    CMrun_network(conn_mgr);
#endif

    MPI_Finalize();

    return 0;
}
