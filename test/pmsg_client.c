#include "../inc/pmsg.h"
#include "pmsg_msg.h"
#include <stdio.h>
#include <string.h>

#define CLIENT_MSG  "this is the client"

int main(void)
{
    struct message msg;

    pmsg_init(sizeof(msg));
    pmsg_attach(PMSG_DAEMON_PID);

    pmsg_open(getpid());

    msg.type = GO;
    msg.pid = getpid();
    memset(msg.text, 0, sizeof(msg.text));
    strncpy(msg.text, CLIENT_MSG, strlen(CLIENT_MSG));

    pmsg_send(PMSG_DAEMON_PID, &msg);

    pmsg_recv(&msg, true);
    printf("text: '%s'\n", msg.text);

    pmsg_detach(PMSG_DAEMON_PID);
    pmsg_close();
    return 0;
}
