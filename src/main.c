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

/* Project includes */
#include <io/nw.h>
#include <alloc.h>
#include <debug.h>

/* Globals */

/* Functions */

int main(int argc, char *argv[])
{
    struct message m;

    printd("Verbose printing enabled\n");

    if (mem_init() < 0) /* The mem interface will start the IO inteface. */
        abort();
    if (mem_launch() < 0)
        abort();

    /* TODO Start the MQ, and alloc interfaces. */
    /* TODO connect alloc export with mq import */

    if (nw_get_rank() == 0)
    {
        m.type = MSG_REQ_ALLOC;
        m.status = MSG_REQUEST;
        m.pid = getpid();
        m.rank = nw_get_rank();
        if (mem_new_request(&m) < 0)
            printd("Error submitting new req\n");
    }

    sleep(2);
    mem_fin();
    return 0;
}
