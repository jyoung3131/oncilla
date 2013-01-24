#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <io/rdma.h>
#include "../src/rdma.h"

#include "ib_defines.h"

void spin_test(void)
{
    srand(time(NULL));

    //char *text = strdup("client wrote some data to buffer");
    char *text = strdup("____________________________________________________");
    size_t text_len = strlen(text);

}

int main(int argc, char *argv[])
{
    ib_t ib;
    struct ib_params params;
    struct ib_msg *msg = calloc(1, sizeof(*msg));
    uint64_t id;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s serverDNS\n", argv[0]);
        return -1;
    }

    params.addr = argv[1];
    params.port = 12345;
    params.buf = msg;
    params.buf_len = sizeof(*msg);

    if (ib_init()) {
        fprintf(stderr, "error: ib_init\n");
        return -1;
    }

    if (!(ib = ib_new(&params))) {
        fprintf(stderr, "error: ib_new\n");
        return -1;
    }

    printf("connecting to %s...\n", argv[1]);
    if (ib_connect(ib, false/*is client*/)) {
        fprintf(stderr, "error: ib_connect\n");
        return -1;
    }
    printf("connected to %s\n", argv[1]);

    msg->op = OP_CONTINUE;
    strncpy(msg->text, text, text_len);

    for (id = 1UL; id <= 10UL; id++) {
        msg->id = id;

        printf("sending msg %02lu: '%s'\n", msg->id, msg->text);
        fflush(stdout);
        if (ib_write(ib, sizeof(*msg))) {
            fprintf(stderr, "error: ib_write\n");
            return -1;
        }
        sleep(1);

        /* modify a random character with a random character */
        text[rand() % text_len] = (rand() % ('Z' - 'A' + 1)) + 'A';
        strncpy(msg->text, text, text_len);
    }

    /* send last message to tell server to stop looping */
    msg->op = OP_STOP;
    msg->text[0] = '\0'; /* empty string */
    msg->id++;
    if (ib_write(ib, sizeof(*msg))) {
        fprintf(stderr, "error: ib_write\n");
        return -1;
    }


    return 0;
}
