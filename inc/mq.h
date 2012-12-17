/**
 * @file attach.h
 * @author Alex Merritt, merritt.alex@gatech.edu
 * @date 2012-08-25
 * @brief Interface for the interposer library to communicate with the shadowfax
 * daemon.
 */

#ifndef _ATTACH_H
#define _ATTACH_H

#include <stdbool.h>
#include <unistd.h>
#include <mqueue.h>

/* mq names must start with / */
#define ATTACH_NAME_PREFIX      "/ocm_mq_"
#define ATTACH_DAEMON_MQ_NAME   ATTACH_NAME_PREFIX "daemon"

#define MAX_LEN				255

/* callback definitions */
typedef enum
{
    /* daemon recv events */
    OCM_CONNECT = 1,
    OCM_DISCONNECT,
    /* interposer recv events */
    OCM_CONNECT_ALLOW
} msg_event;

struct mq_state; /* forward declaration */

/* e = message kind / event
 * pid = process sending daemon a message
 * data = some data sent by app to daemon within message, depends on e
 */
typedef void (*msg_recv_callback)(msg_event e, pid_t pid, void *data);

/* connection state */
struct mq_state
{
    bool valid;
    char name[MAX_LEN];
    pid_t pid;
    mqd_t id;
    msg_recv_callback notify;
};

/* daemon functions */
int attach_clean(void);
int attach_open(msg_recv_callback notify);
int attach_close(void);
int attach_allow(struct mq_state *state, pid_t pid);
int attach_dismiss(struct mq_state *state);
int attach_send_allow(struct mq_state *state, bool allow);
//int attach_send_assembly(struct mq_state *state, assembly_key_uuid key);

/* interposer functions */
/* interposer does not receive asynchronous message notification from daemon */
int attach_init(struct mq_state *recv, struct mq_state *send);
int attach_tini(struct mq_state *recv, struct mq_state *send);
int attach_send_connect(struct mq_state *recv, struct mq_state *send);
int attach_send_disconnect(struct mq_state *recv, struct mq_state *send);
//int attach_send_request(struct mq_state *recv, struct mq_state *send,
        //struct assembly_hint *hint, assembly_key_uuid key);

/* if code crashes, call this to remove files this interface may have created
 * which were not cleaned up */
void attach_cleanup(void);

#endif /* _ATTACH_H */
