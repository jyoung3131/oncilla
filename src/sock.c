/**
 * @file sock.c
 * @author Alex Merritt, merritt.alex@gatech.edu
 * @date 2011-11-20
 * @brief Reused code from Shadowfax network io module
 */

// System includes
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sock.h>

/*-------------------------------------- PUBLIC FUNCTIONS --------------------*/

int conn_connect(
		struct sockconn *conn,
		const char *host_ip, const char *host_port)
{
	int err, exit_errno;

	struct addrinfo hints, *results = NULL;

	// Initialize the structure
	memset(conn, 0, sizeof(*conn));
	conn->socket = -1;

	// Query host information.
	memset(&hints, 0, sizeof(hints));
	hints.ai_family      = AF_UNSPEC;
	hints.ai_socktype    = SOCK_STREAM;
	err = getaddrinfo(host_ip, host_port, &hints, &results);
	if (err != 0 || !results) {
		exit_errno = -ENETDOWN;
		goto fail;
	}

	// Store the first result to our connection structure.
	conn->address_family = results->ai_family;
	conn->socktype = results->ai_socktype;
	conn->protocol = results->ai_protocol;
	conn->address_len = results->ai_addrlen;
	conn->address = calloc(1, results->ai_addrlen);
	if(!conn->address) {
		exit_errno = -ENOMEM;
		goto fail;
	}
	memcpy(conn->address, results->ai_addr, conn->address_len);
	freeaddrinfo(results);
	results = NULL;

	// Obtain a new socket from the kernel.
    conn->socket = socket(conn->address_family, conn->socktype, conn->protocol);
	if(conn->socket < 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}

	// Establish a TCP connection.
	err = connect(conn->socket, (struct sockaddr *)conn->address, conn->address_len);
	if (err != 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}

	return 0;

fail:
	if(conn->socket > -1)
		close(conn->socket);
    conn->socket = -1;
	if(results)
		freeaddrinfo(results);
	return exit_errno;
}

int conn_close(struct sockconn *conn)
{
	if (!conn || conn->socket < 0)
		return -EINVAL;
	close(conn->socket);
	conn->socket = -1;
	if(conn->address)
		free(conn->address);
	conn->address = NULL;
	return 0;
}

int conn_localbind(struct sockconn *conn, const char *bind_port)
{
	int err, exit_errno;
	int serverfd = -1; /* parent socket */
	int optval; /* flag value for setsockopt */

	struct addrinfo hints, *servaddrinfo = NULL;
	char servername[255];

	if(!conn || !bind_port) {
		exit_errno = -EINVAL;
		goto fail;
	}

	memset(conn, 0, sizeof(*conn));

	memset(servername, 0, 255 * sizeof(char));
	gethostname(servername, 255);

	// FIXME This code needs to change to support binding to a specific IP
	// address, instead of all IP addresses this host associates with. That way
	// we can have separate sockets listening to the same port but on different
	// externally visible IP addresses (e.g. one for SDP, one for Ethernet).

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	err = getaddrinfo(NULL, bind_port, &hints, &servaddrinfo);
	if(err != 0) {   
		exit_errno = -ENETDOWN;
		goto fail;
	}
    serverfd = socket(servaddrinfo->ai_family,
            servaddrinfo->ai_socktype, servaddrinfo->ai_protocol);
	if (serverfd < 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}
	optval = 1;
	err = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR,
			(const void *)&optval , sizeof(int));
	if(err < 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}   
	err = bind(serverfd, (struct sockaddr *)servaddrinfo->ai_addr,
			servaddrinfo->ai_addrlen);
	if(err < 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}

	// TODO probably don't need these in the future
	conn->socket         = serverfd;
	conn->socktype       = servaddrinfo->ai_socktype;
	conn->protocol       = servaddrinfo->ai_protocol;
	conn->address_len    = servaddrinfo->ai_addrlen;
	conn->address_family = servaddrinfo->ai_family;
	conn->address        = calloc(1, servaddrinfo->ai_addrlen);
	if(!conn->address) {
		exit_errno = -ENOMEM;
		goto fail;
	}
	memcpy(conn->address, servaddrinfo->ai_addr, servaddrinfo->ai_addrlen );
	freeaddrinfo(servaddrinfo);
	servaddrinfo = NULL;

	// Listen for incoming requests; not a blocking call
	err = listen(serverfd, 10); // FIXME do something with backlog arg
	if(err < 0) {
		exit_errno = -ENETDOWN;
		goto fail;
	}

	return 0;

fail:
	if(serverfd > -1)
		close(serverfd);
	if(servaddrinfo)
		freeaddrinfo(servaddrinfo);
	return exit_errno;
}

int conn_accept(struct sockconn *conn, struct sockconn *new_conn)
{
	int exit_errno;
	if (!conn || !new_conn) {
		exit_errno = -EINVAL;
		goto fail;
	}
	// wait for a new connection - blocking call
	memset(&new_conn->address, 0, sizeof(struct sockaddr_in));
	new_conn->address_len = 0;
	new_conn->socket =
		accept(	conn->socket,
				(struct sockaddr *)&new_conn->address,
				&new_conn->address_len);
	if (new_conn->socket < 0) {
		exit_errno = -(errno);
		goto fail;
	}
	return 0;
fail:
	return exit_errno;
}

int conn_peername(struct sockconn *conn, char *hostname)
{
	int err;
	struct sockaddr_in *addr;
	struct sockaddr_storage peer;
	socklen_t len = sizeof(peer);
	err = getpeername(conn->socket, (struct sockaddr*)&peer, &len);
	if (err < 0)
		return -(errno);
	//BUG(peer.ss_family != AF_INET); // we only support IPv4
	addr = (struct sockaddr_in*)&peer;
	inet_ntop(AF_INET, &(addr->sin_addr), hostname, INET_ADDRSTRLEN);
	return 0;
}

int conn_put(struct sockconn *conn, void *data, int len)
{
	char *data_ptr = (char *)data; // place holder in data
	int sent = 0;                  // bytes transferred on each send()
	int remain = len;              // bytes (<= len) remaining to send

	if(!conn || conn->socket < 0 || !data || len < 0) {
		return -EINVAL;
	}
	while(remain > 0) {
		sent = send(conn->socket, data_ptr, remain, 0);
		if(sent <= 0)
			return sent; // error (< 0) or finished (= 0)
		remain -= sent;
		data_ptr += sent;
	}

	return 1;
}

int conn_get(struct sockconn *conn, void *data, int len)
{
	char *data_ptr = (char *)data; // place holder in data
	int recvd = 0;                 // bytes transferred on each recv()
	int remain = len;              // bytes (<= len) remaining to recv

	if(!conn || conn->socket < 0 || !data || len < 0)
		return -EINVAL;

	while(remain > 0) {
		recvd = recv(conn->socket, data_ptr, remain, 0);
		if(recvd <= 0)
			return recvd; // error (< 0) or remote end closed on us (= 0)
		remain -= recvd;
		data_ptr += recvd;
	}

	return 1;
}

bool conn_is_connected(struct sockconn *conn)
{
    bool ret = false;
    if (conn && conn->socket >= 0)
        ret = true;
    return ret;
}
