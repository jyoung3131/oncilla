/**
 * @file attach.h
 * @author Alex Merritt, merritt.alex@gatech.edu
 * @date 2012-08-25
 * @brief Interface for the interposer library to communicate with the shadowfax
 * daemon.
 */

#ifndef __ATTACH_H__
#define __ATTACH_H__

/* System includes */
#include <stdbool.h>
#include <unistd.h>
#include <mqueue.h>

/* Other project includes */

/* Project includes */
#include <msg.h>

/* Defines */

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
    OCM_CONNECT_ALLOW,
    /* user-defined, i.e. examine message data */
    OCM_USER
} msg_event;

struct mq_state; /* forward declaration */

/* e = message kind / event
 * pid = process sending daemon a message
 * data = some data sent by app to daemon within message, depends on e
 */
typedef void (*msg_recv_callback)(msg_event e, pid_t pid, void *data);

/* daemon functions */

int attach_clean(void);
int attach_open(msg_recv_callback notify);
int attach_close(void);
int attach_allow(pid_t pid);
int attach_dismiss(pid_t pid);
int attach_send_allow(pid_t pid, bool allow);
struct message_forward attach_get_import(void);

/* client functions */

/* client does not receive asynchronous message notification from daemon, and
 * only interacts with the daemon, not other clients (i.e. attach_send will send
 * a message to the daemon). */

int attach_init(msg_recv_callback notify);
int attach_tini(void);
int attach_send_connect(void);
int attach_send_disconnect(void);
/* for user-defined messages */
int attach_send(struct message *u_msg);
int attach_recv(struct message *u_msg);

/* if code crashes, call this to remove files this interface may have created
 * which were not cleaned up */
void attach_cleanup(void);

#endif /* __ATTACH_H__ */
