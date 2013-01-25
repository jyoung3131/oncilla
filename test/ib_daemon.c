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
        usleep(500);

    /* verify all elements have been updated */
    size_t i;
    for (i = 0; i < count; i++)
        if (buf[i] != 0xdeadbeef)
            fprintf(stderr, "x");

    /* XXX need to implement teardown() */

    return 0;
}

/* Allocate an array of strings. Client will update one of them, we respond by
 * updating another. Client will spin until it sees our changes, by periodically
 * reading from our buffer.
 */
static int buffer_size_mismatch_test(void)
{
    ib_t ib;
    struct ib_params params;
    struct {
        char str[32];
    } *buf = NULL;
    size_t count = 8;
    size_t len = count * sizeof(*buf);
    unsigned int times;
    bool is_equal;

    if (!(buf = calloc(count, sizeof(*buf))))
        return -1;

    params.addr     = NULL;
    params.port     = 12345;
    params.buf      = buf;
    params.buf_len  = len;

    if (!(ib = setup(&params)))
        return -1;

    /* wait for client to update us */
    times = 1000;
    char recv[] = "hello";
    char resp[] = "nice to meet you";
    do {
        usleep(500);
        is_equal = (strncmp(buf[2].str, recv, strlen(recv)) == 0);
    } while (--times > 0 && !is_equal);
    if (times == 0)
        return -1;

    /* create response. client will be pulling */
    strncpy(buf[7].str, resp, strlen(resp) + 1);

    /* client will write, telling us they got the response */
    times = 1000;
    while (--times > 0 && buf[0].str[0] != '\0')
        usleep(500);
    if (times == 0)
        return -1;

    /* XXX need to implement teardown() */

    return 0;
}

// TODO bandwidth test

// TODO Multiple connections test

// TODO Multiple regions test

int main(int argc, char *argv[])
{
    if (argc != 2) {
usage:
        fprintf(stderr, "Usage: %s test_id\n\ttest_id: [0-1]\n", *argv);
        return -1;
    }
    switch (atoi(argv[1])) {
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
