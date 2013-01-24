#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <io/rdma.h>
#include "../src/rdma.h"

#include "ib_defines.h"

void spin_test(void)
{
    uint64_t last_id = 0UL;
    while (msg->op != OP_STOP) {
        if (msg->id <= last_id) { usleep(100); continue; }
        last_id = msg->id;
        printf("new msg %02lu: '%s'\n", msg->id, msg->text);
    }

}

int main(void)
{
    ib_t ib;
    struct ib_params params;
    volatile struct ib_msg *msg = calloc(1, sizeof(*msg));

    params.addr = strdup("asdf");
    params.port = 12345;
    params.buf = (void*)msg;
    params.buf_len = sizeof(*msg);

    if (ib_init()) {
        fprintf(stderr, "error: ib_init\n");
        return -1;
    }

    if (!(ib = ib_new(&params))) {
        fprintf(stderr, "error: ib_new\n");
        return -1;
    }

    printf("waiting for connection...\n");
    if (ib_connect(ib, true/*is server*/)) {
        fprintf(stderr, "error: ib_connect\n");
        return -1;
    }
    printf("got connection\n");

    /*
     * RDMA reads and writes will be initiated by the client without
     * our being aware ...
     *
     * Loop until the buffer has been modified by the client
     * indicating a stop.
     */
    spin_test();

    return 0;
}
