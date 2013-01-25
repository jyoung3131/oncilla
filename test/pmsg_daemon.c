#include "../inc/pmsg.h"
#include "pmsg_msg.h"
#include <stdio.h>
#include <string.h>

#define DAEMON_MSG "this is the daemon"

int main(void)
{
    struct message msg;
    pid_t pid;

    pmsg_cleanup();

    pmsg_init(sizeof(msg));
    pmsg_open(PMSG_DAEMON_PID);

    pmsg_recv(&msg, true);
    pid = msg.pid;
    printf("msg: '%s'\n", msg.text);

    pmsg_attach(pid);
    memset(msg.text, 0, sizeof(msg.text));
    strncpy(msg.text, DAEMON_MSG, strlen(DAEMON_MSG));
    pmsg_send(pid, &msg);

    pmsg_detach(pid);
    pmsg_close();
    return 0;
}
