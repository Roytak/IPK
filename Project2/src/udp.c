/*
 * IPK - Project 2 (IOTA)
 * File: udp.c
 * Desc: UDP server functionality
 * Author: Roman Janota
 * Login: xjanot04
*/

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"
#include "parser.h"

extern struct server_opts server_opts;

extern volatile int exit_application;

/* initialize the UDP server */
static int
udp_init_server(const char *address, int port)
{
	int sock, ret = 0;
	struct sockaddr_in sa;
	const int reuse_addr = 1;

	/* create new socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		ERR("Creating server socket failed (%s).", strerror(errno));
		goto cleanup;
	}

	/* set socket options */
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof reuse_addr);
	if (ret < 0) {
		ERR("Setsockopt failed (%s).", strerror(errno));
		close(sock);
		return -1;
	}

	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(address);
	sa.sin_port = htons(port);

	/* bind the socket to an address */
	ret = bind(sock, (struct sockaddr *) &sa, sizeof(sa));
	if (ret) {
		ERR("Bind failed (%s).", strerror(errno));
		close(sock);
		goto cleanup;
	}

cleanup:
	return sock;
}

/* make a response to an udp request */
static int
udp_create_response(char buffer[MAX_BUFFER_SIZE], int err)
{
	int ret = 0;
	struct node *tree = NULL;
	char *ans = NULL;
	int len;
	char copy[MAX_BUFFER_SIZE];

	/* create a copy and reset the buffer, since it will store the response */
	memcpy(copy, buffer, MAX_BUFFER_SIZE);
	memset(buffer, 0, MAX_BUFFER_SIZE);

	if (err < 0) {
		/* create error response */
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = strlen("Invalid request.\n");
		strcpy(buffer + 3, "Invalid request.\n");
		len = strlen("Invalid request.\n");
		goto cleanup;
	}

	/* create new tree for calculation of the answer */
	ret = new_tree(copy + 2, &tree);
	if (ret) {
		ERR("Creating new tree failed.");
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = strlen("Internal error.\n");
		strcpy(buffer + 3, "Internal error.\n");
		len = strlen("Internal error.\n");
		goto cleanup;
	}

	/* get the answer */
	ret = calculate_answer(tree);
	if (ret == INT_MIN) {
		/* division by zero */
		ERR("Calculation failed (division by zero).");
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = strlen("Calculation failed (division by zero).\n");
		strcpy(buffer + 3, "Calculation failed (division by zero).\n");
		len = strlen("Calculation failed (division by zero).\n");
		goto cleanup;
	} else if (ret < 0) {
		/* negative result */
		ERR("Calculation failed (negative result).");
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = strlen("Calculation failed (negative result).\n");
		strcpy(buffer + 3, "Calculation failed (negative result).\n");
		len = strlen("Calculation failed (negative result).\n");
		goto cleanup;
	}

	/* convert the answer to a string */
	asprintf(&ans, "%d", ret);
	if (!ans) {
		ERR("Memory allocation error.");
		buffer[0] = 1;
		buffer[1] = 1;
		buffer[2] = strlen("Internal error.\n");
		strcpy(buffer + 3, "Internal error.\n");
		len = strlen("Internal error.\n");
		goto cleanup;
	}

	/* everything went well, prepare answer */
	buffer[0] = 1;
	buffer[1] = 0;
	len = strlen(ans);
	buffer[2] = len;
	strcpy(buffer + 3, ans);
	free(ans);

cleanup:
	del_tree(tree);
	return len + 3;
}

/* main UDP loop */
int
handle_udp()
{
	int ret, server_sock;
	char buffer[MAX_BUFFER_SIZE];
	int client_socks[MAX_CLIENTS];
	struct sockaddr_in client_addr;
	int addrlen;
	fd_set readfds;
	ssize_t bytes;

	/* initialize the server */
	server_sock = udp_init_server(server_opts.address, server_opts.port);
	if (server_sock < 0) {
		ERR("Initializing UDP server failed.");
		return -1;
	}

	addrlen = sizeof client_addr;
	memset(&client_socks, 0, sizeof client_socks);

	while(!exit_application) {
		/* reset the buffer */
		memset(buffer, 0, MAX_BUFFER_SIZE);

		FD_ZERO(&readfds);
		FD_SET(server_sock, &readfds);

		/* wait for the server socket to be ready */
		ret = select(server_sock + 1, &readfds, NULL, NULL, NULL);
		if (ret < 0) {
			if (errno == EINTR) {
    			ret = 0;
    			goto cleanup;
    		}

			ERR("Select failed (%s).", strerror(errno));
			ret = 1;
			goto cleanup;
		}

		/* check for new connection */
		if (FD_ISSET(server_sock, &readfds)) {
			bytes = recvfrom(server_sock, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
			if (bytes < 0) {
				ERR("Recvfrom failed (%s).", strerror(errno));
				ret = 1;
				goto cleanup;
			}

			/* parse the request and get the length of it */
			ret = udp_parse_request(buffer, bytes);
			if (ret < 0) {
				ERR("Unexpected message (%s).", buffer);
			} else {
				buffer[ret + 2] = '\0';
			}

			/* creates the response, which reflects the result of parsing */
			ret = udp_create_response(buffer, ret);

			if (sendto(server_sock, buffer, ret, 0, (struct sockaddr *)&client_addr, addrlen) < 0) {
                ERR("Sendto failed (%s).", strerror(errno));
                ret = 1;
                goto cleanup;
            }
		}
	}

cleanup:
	return ret;
}

