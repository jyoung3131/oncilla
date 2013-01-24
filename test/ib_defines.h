#ifndef __IB_DEFINES_H__
#define __IB_DEFINES_H__

#define MSG_LEN 2048

enum op {
    OP_CONTINUE = 1,
    OP_STOP
};

struct ib_msg {
    enum op     op;
    uint64_t    id;
    char        text[MSG_LEN];
};

#endif
