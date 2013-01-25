#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <io/rdma.h>
#include "../src/rdma.h"

static ib_t setup(struct ib_params *p)
{
    ib_t ib = NULL;

    if (ib_init())
        return (ib_t)NULL;

    if (!(ib = ib_new(p)))
        return (ib_t)NULL;

    if (ib_connect(ib, true/*is server*/))
        return (ib_t)NULL;

    return ib;
}


static int one_sided_test(void)
{
    ib_t ib;
    struct ib_params params;
    unsigned int *buf = NULL;
    size_t count = (1 << 10);
    size_t len = count * sizeof(*buf);

    if (!(buf = calloc(count, sizeof(*buf))))
        return -1;

    params.addr     = NULL;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
        return -1;

    /* wait for client to write entire buffer */
    while (buf[count - 1] == 0)
        ;

    /* verify all elements have been updated */
    size_t i;
    for (i = 0; i < count; i++)
        if (buf[i] != 0xdeadbeef)
            fprintf(stderr, "x");

    /* XXX need to implement teardown() */

    return 0;
}

static int buffer_size_mismatch_test(void)
{
    return -1;
}

static int bandwidth_test(void)
{
    return -1;
}

// TODO Multiple connections test

// TODO Multiple regions test

int main(void)
{
    if (one_sided_test()) {
        fprintf(stderr, "one_sided_test failed\n");
        return -1;
    }
    return 0;
}
