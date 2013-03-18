/**
 * @file pmsg.h
 * @author Alex Merritt, merritt.alex@gatech.edu
 * @date 2012-08-25
 * @brief interface for processes to send messages using 'mailboxes'
 */

#ifndef __PMSG_H__
#define __PMSG_H__

/* System includes */
#include <stdbool.h>
#include <unistd.h>
#include <mqueue.h>

/* Other project includes */

/* Project includes */

/* Defines */

/* mq names must start with a forward-slash / */
#define ATTACH_NAME_PREFIX      "/ocm_mq_"
#define ATTACH_DAEMON_MQ_NAME   ATTACH_NAME_PREFIX "daemon"

#define MAX_LEN				255

#define PMSG_DAEMON_PID     (-1)

/* pmsg assumes singular message size throughout */
int pmsg_init(size_t pmsg_size);

/* open/close self mailbox for receiving messages */
/* only one receive mailbox supported */
int pmsg_open(pid_t self_pid);
int pmsg_close(void);

/* attach to/detach from other's mailbox for sending messages */
int pmsg_attach(pid_t to_pid);
int pmsg_detach(pid_t to_pid);

/* send a message to a mailbox previously attached */
int pmsg_send(pid_t to_pid, void *msg);
/* receive a message from within self mailbox */
int pmsg_recv(void *msg, bool block);

/* clean lingering pmsg mailboxes in the system */
int pmsg_cleanup(void);

/* number of messages pending in receive queue */
int pmsg_pending(void);

#endif /* __PMSG_H__ */
