#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <debug.h>

#include <io/rdma.h>
#include "../src/rdma.h"

static char *serverIP = NULL;

static ib_t setup(struct ib_params *p)
{
    ib_t ib;

    if (ib_init())
        return (ib_t)NULL;

    if (!(ib = ib_new(p)))
        return (ib_t)NULL;

    if (ib_connect(ib, false/*is client*/))
        return (ib_t)NULL;

    return ib;
}

static int teardown(void)
{
    return -1;
}

/* Does a simple write/read to/from remote memory. */
static int one_sided_test(void)
{
    ib_t ib;
    struct ib_params params;
    unsigned int *buf = NULL;
    size_t count = (1 << 10);
    size_t len = count * sizeof(*buf);
    size_t i;

    if (!(buf = calloc(count, sizeof(*buf))))
        return -1;

    params.addr     = serverIP;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
        return -1;

    for (i = 0; i < count; i++)
        buf[i] = 0xdeadbeef;

    /* send and wait for completion */
    if (ib_write(ib, len) || ib_poll(ib))
        return -1;

    memset(buf, 0, len);

    /* read back and wait for completion */
    if (ib_read(ib, len) || ib_poll(ib))
        return -1;

    for (i = 0; i < count; i++)
        if (buf[i] != 0xdeadbeef)
            return -1;

    /* XXX need to implement teardown() */

    return 0; /* test passed */
}

static int buffer_size_mismatch_test(void)
{
    return -1;
}

static int bandwidth_test(void)
{
    return -1;
}

// TODO Multiple connections test (many clients)

// TODO Multiple regions test (one client many MRs)

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s server_ip\n", argv[0]);
        return -1;
    }

    serverIP = argv[1];

    if (one_sided_test()) {
        fprintf(stderr, "FAIL: one_sided_test\n");
        return -1;
    } else
        printf("pass: one_sided_test\n");

    return 0;
}
