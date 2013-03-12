/**
 * This file is meant as a temporary replacement for networking code until HATI
 * works. If not already obvious, it only uses sockets.
 *
 * @file sockets.h
 * @author Alex Merritt, merritt.alex@gatech.edu
 * @date 2011-11-17
 */

#ifndef _SOCKETS_H
#define _SOCKETS_H

// System includes
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>

struct sockconn
{
	struct sockaddr_in *address;
	socklen_t address_len;
	int address_family;
	int socket, socktype, protocol;
};

// client ----------------------------
int conn_connect(
		struct sockconn *conn,
		const char *host_ip, const char *host_port);

// server ----------------------------
int conn_localbind(struct sockconn *conn, const char *bind_port);
int conn_accept(struct sockconn *conn, struct sockconn *new_conn);

// common ----------------------------
int conn_close(struct sockconn *conn);
// returns < 0 on error, = 0 if remote socket closed, > 0 if okay
int conn_put(struct sockconn *conn, void *data, int len);
// returns < 0 on error, = 0 if remote socket closed, > 0 if okay
int conn_get(struct sockconn *conn, void *data, int len);
// writes out hostname (length >= INET_ADDRSTRLEN)
// of peer connecting to us via 'conn'
int conn_peername(struct sockconn *conn, char *hostname);

#endif
