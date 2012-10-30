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
#include <mem.h>
#include <debug.h>

/* Globals */

/* Functions */

int main(int argc, char *argv[])
{
    printd("Verbose printing enabled\n");

    /* The mem interface will start the IO inteface. */
    if (mem_init() < 0)
        ABORT();

    /* TODO Start the MQ, and alloc interfaces. */

    /* TODO connect mem export with mq import */

    if (mem_launch() < 0)
        ABORT();


#if 1 /* to test sending a message without the MQ module or apps */
    struct message m;
    m.type = MSG_REQ_ALLOC;
    m.status = MSG_REQUEST;
    m.pid = getpid();
    m.rank = nw_get_rank();
    m.u.req.orig_rank = m.rank;
    m.u.req.bytes = (4 << 10);
    if (m.rank > 0)
        if (mem_new_request(&m) < 0)
            printd("Error submitting new req\n");
#endif

    /* TODO Need to wait on signal or something instead of sleeping */
    sleep(2);

    mem_fin();
    return 0;
}
