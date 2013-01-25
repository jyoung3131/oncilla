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
    if (ib_write(ib, 0, len) || ib_poll(ib))
        return -1;

    memset(buf, 0, len);

    /* read back and wait for completion */
    if (ib_read(ib, 0, len) || ib_poll(ib))
        return -1;

    for (i = 0; i < count; i++)
        if (buf[i] != 0xdeadbeef)
            return -1;

    /* XXX need to implement teardown() */

    return 0; /* test passed */
}

/* Allocate one string, server will allocate an array of strings. We update ours
 * and write it into one of the server's. Server sees this, then updates another
 * string in the array; we periodically pull and return when we see the change.
 */
static int buffer_size_mismatch_test(void)
{
    ib_t ib;
    struct ib_params params;
    struct {
        char str[32];
    } *buf = NULL;
    size_t len = sizeof(*buf);
    unsigned int times;
    bool is_equal;

    if (!(buf = calloc(1, len)))
        return -1;

    params.addr     = serverIP;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
        return -1;

    /* send 'hello' to second index (third string) */
    /* wait for 'nice to meet you' in seventh (last) index */
    /* write back, releasing server */

    strncpy(buf->str, "hello", strlen("hello") + 1);
    if (ib_write(ib, (2 * len), len) || ib_poll(ib))
        return -1;

    times = 1000;
    char resp[] = "nice to meet you";
    do {
        usleep(500);
        if (ib_read(ib, (7 * len), len) || ib_poll(ib))
            return -1;
        is_equal = (strncmp(buf->str, resp, strlen(resp)) != 0);
    } while (--times > 0 && !is_equal);

    buf->str[0] = '\0';
    if (ib_write(ib, 0, len) || ib_poll(ib))
        return -1;

    /* XXX need to implement teardown() */

    return 0;
}

// TODO bandwidth test

// TODO Multiple connections test (many clients)

// TODO Multiple regions test (one client many MRs)

int main(int argc, char *argv[])
{
    if (argc != 3) {
usage:
        fprintf(stderr, "Usage: %s server_ip test_id\n"
                "\ttest_id: [0-1]\n", argv[0]);
        return -1;
    }
    serverIP = argv[1];
    switch (atoi(argv[2])) {
    case 0:
        if (one_sided_test()) {
            fprintf(stderr, "FAIL: one_sided_test\n");
            return -1;
        } else
            printf("pass: one_sided_test\n");
        break;
    case 1:
        if (buffer_size_mismatch_test()) {
            fprintf(stderr, "FAIL: buffer_size_mismatch_test\n");
            return -1;
        } else
            printf("pass: buffer_size_mismatch_test\n");
        break;
    default:
        goto usage; /* >:) */
    }
    return 0;
}
