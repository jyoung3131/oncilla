#ifndef PMSG_H
#define PMSG_H

enum msgtype
{
    GO = 0,
    STOP
};

struct message
{
    enum msgtype type;
    pid_t pid;
    char text[256];
};

#endif
