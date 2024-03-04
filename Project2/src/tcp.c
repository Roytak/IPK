/*
 * IPK - Project 2 (IOTA)
 * File: tcp.c
 * Desc: TCP server functionality
 * Author: Roman Janota
 * Login: xjanot04
*/

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "server.h"

extern struct server_opts server_opts;

extern volatile int exit_application;

/* initializes the server */
static int
tcp_init_server(const char *address, int port)
{
	int sock, ret = 0, flags;
	struct sockaddr_in sa;
	const int reuse_addr = 1;

	/* create new socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		ERR("Creating server socket failed (%s).", strerror(errno));
		goto cleanup;
	}

	/* make the socket non-blocking */
	flags = fcntl(sock, F_GETFL);
	if (flags == -1) {
		ERR("Getting socket options failed.");
		close(sock);
		return -1;
	}

	flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	if (flags == -1) {
		ERR("Setting socket options failed.");
		close(sock);
		return -1;
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

	/* bind the server to an address */
	ret = bind(sock, (struct sockaddr *) &sa, sizeof(sa));
	if (ret) {
		ERR("Bind failed (%s).", strerror(errno));
		close(sock);
		goto cleanup;
	}

	/* listen on this socket */
	ret = listen(sock, SOCKET_BACKLOG);
	if (ret) {
		ERR("Listen failed (%s).", strerror(errno));
		close(sock);
		goto cleanup;
	}

cleanup:
	return sock;
}

static int
accept_new_connection(int sock)
{
    int client_sock, flags;
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    /* wait for the server socket to become readable, with a timeout of one second */
    if (select(sock + 1, &readfds, NULL, NULL, &timeout) < 0) {
    	if (errno == EINTR) {
    		return 0;
    	}

        ERR("Select failed (%s).", strerror(errno));
        return -1;
    }

    /* if the server socket is readable, accept the incoming connection */
    if (FD_ISSET(sock, &readfds)) {
        client_sock = accept(sock, NULL, NULL);
        if (client_sock < 0) {
            ERR("Accept failed (%s).", strerror(errno));
            return -1;
        }

        printf("New connection accepted.\n");

        /* make the socket non-blocking */
        flags = fcntl(client_sock, F_GETFL);
        if (flags == -1) {
            ERR("Getting socket options failed.");
            close(client_sock);
            return -1;
        }

        flags = fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
        if (flags == -1) {
            ERR("Setting socket options failed.");
            close(client_sock);
            return -1;
        }

        return client_sock;
    }

    return -1;
}

/* creates new context */
static struct context *
ctx_new(int sock)
{
	struct context *ctx;

	ctx = calloc(1, sizeof *ctx);
	if (!ctx) {
		ERR("Memory allocation error.");
		return NULL;
	}

	ctx->sock = sock;
	ctx->state = INIT;

	return ctx;
}

static int
tcp_init(struct context *ctx)
{
	int ret = 0;
	fd_set readfds;
	int already_read = 0;

	while (1) {
		FD_ZERO(&readfds);
        FD_SET(ctx->sock, &readfds);

        /* wait for incoming data from the client */
        ret = select(ctx->sock + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
        	if (errno == EINTR) {
    			return 0;
    		}

            ERR("Select failed (%s).", strerror(errno));
            goto cleanup;
        }

        /* if the client is ready, read the data */
        if (FD_ISSET(ctx->sock, &readfds)) {
            ret = recv(ctx->sock, ctx->buffer + already_read, MAX_BUFFER_SIZE - already_read, 0);
            if (ret < 0) {
                ERR("Recv failed (%s).", strerror(errno));
                goto cleanup;
            } else if (ret == 0) {
                printf("Client disconnected.\n");
                ctx->state = TERM;
                goto cleanup;
            } else {
            	/* got data, check if it's just HELLO\n */
            	if (!strchr(ctx->buffer, '\n')) {
            		/* new line not found */
            		already_read += ret;
            		continue;
            	}
                ctx->buffer[already_read + ret] = '\0';
                if (!strncmp(ctx->buffer, TCP_HELLO, strlen(TCP_HELLO))) {
                	ret = send(ctx->sock, TCP_HELLO, strlen(TCP_HELLO), 0);
                	if (ret == -1) {
                		ERR("Sending hello message failed (%s).", strerror(errno));
                	} else {
                		ctx->state = READ;
                	}
                } else {
                	/* expected hello, got something else */
                	ctx->state = TERM;
                }

                goto cleanup;
            }
        }
    }

cleanup:
    return ret;
}

/* checks the query */
static int
tcp_check_message(struct context *ctx)
{
	int ret = 0;

	/* check length of shortest possible valid message */
	if (strlen(ctx->buffer) < strlen("SOLVE (+ 1 1)\n")) {
		ERR("Invalid message.\n");
		ret = 1;
		ctx->state = TERM;
		goto cleanup;
	}

	/* parse the message */
	ret = tcp_parse_query(ctx->buffer);
	if (ret) {
		ERR("Unexpected message (%s).", ctx->buffer);
		ctx->state = TERM;
		goto cleanup;
	}

cleanup:
	return ret;
}

/* read logic */
static int
tcp_read(struct context *ctx)
{
	int ret = 0;
	fd_set readfds;
	int already_read = 0;

	/* reset the buffer */
	memset(ctx->buffer, 0, MAX_BUFFER_SIZE);

	while (1) {
		FD_ZERO(&readfds);
        FD_SET(ctx->sock, &readfds);

        /* wait for incoming data from the client */
        ret = select(ctx->sock + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
            ERR("Select failed (%s).", strerror(errno));
            goto cleanup;
        }

        /* if the client is ready, read the data */
        if (FD_ISSET(ctx->sock, &readfds)) {
            ret = recv(ctx->sock, ctx->buffer + already_read, MAX_BUFFER_SIZE - already_read, 0);
            if (ret < 0) {
                ERR("Recv failed (%s).", strerror(errno));
                goto cleanup;
            } else if (ret == 0) {
                printf("Client disconnected.\n");
                ctx->state = TERM;
                goto cleanup;
            } else {
            	if (!strchr(ctx->buffer, '\n')) {
            		/* new line not found */
            		already_read += ret;
            		continue;
            	}
                ctx->buffer[already_read + ret] = '\0';
                if (!strncmp(ctx->buffer, TCP_BYE, strlen(TCP_BYE))) {
                	ctx->state = TERM;
                	ret = 0;
                	printf("Sending BYE to client.\n");
                	goto cleanup;
                }

                ret = tcp_check_message(ctx);
                if (!ret) {
                	/* set state to write */
                	ctx->state = WRITE;
                }

                goto cleanup;
            }
        }
    }

cleanup:
    return ret;
}

static int
tcp_get_answer(struct context *ctx, int *answer)
{
	int ret = 0;
	const char *expression;
	struct node *tree = NULL;

	expression = ctx->buffer + strlen("SOLVE ");
	ret = new_tree(expression, &tree);
	if (ret) {
		goto cleanup;
	}

	*answer = calculate_answer(tree);
	if (*answer == INT_MIN) {
		ERR("Calculation failed (0 division).");
		ret = 1;
		goto cleanup;
	} else if (*answer < 0) {
		ERR("Calculation failed (negative result).");
		ret = 1;
		goto cleanup;
	}

cleanup:
	del_tree(tree);
	return ret;
}

static int
tcp_write(struct context *ctx)
{
	int ret = 0, answer = 0;
	fd_set writefds;

	/* get the answer to the query */
	ret = tcp_get_answer(ctx, &answer);
	if (ret) {
		ERR("Getting answer failed.");
		ctx->state = TERM;
		goto cleanup;
	}

	/* reset the buffer and set it to the answer */
	memset(ctx->buffer, 0, MAX_BUFFER_SIZE);
	sprintf(ctx->buffer, "RESULT %d\n", answer);

	while (1) {
		FD_ZERO(&writefds);
        FD_SET(ctx->sock, &writefds);

        /* wait for the client to be ready for writing */
        ret = select(ctx->sock + 1, NULL, &writefds, NULL, NULL);
        if (ret < 0) {
            ERR("Select failed (%s).", strerror(errno));
            goto cleanup;
        }

        /* if the client socket is writeable, write answer */
        if (FD_ISSET(ctx->sock, &writefds)) {
            ret = send(ctx->sock, ctx->buffer, MAX_BUFFER_SIZE, 0);
            if (ret < 0) {
                ERR("Send failed (%s).", strerror(errno));
                goto cleanup;
            } else if (ret == 0) {
                printf("Client disconnected.\n");
                ctx->state = TERM;
                goto cleanup;
            } else {
            	ctx->state = READ;
            	ret = 0;
                goto cleanup;
            }
        }
    }

cleanup:
    return ret;
}

/* terminates the connection and the underlying thread */
static void
tcp_term(struct context *ctx)
{
	int ret;

	ret = send(ctx->sock, TCP_BYE, strlen(TCP_BYE), 0);
	if (ret == -1) {
		ERR("Sending bye message failed (%s).", strerror(errno));
	}

	close(ctx->sock);
	ctx->sock = -1;
	free(ctx);

	pthread_exit(NULL);
}

/* logic for a TCP session */
static void *
tcp_session(void *arg)
{
	int ret = 0;
	struct context *ctx = arg;

	while (1) {
		/* get current state */
		switch (ctx->state) {
		case INIT:
			ret = tcp_init(ctx);
			break;
		case READ:
			ret = tcp_read(ctx);
			break;
		case WRITE:
			ret = tcp_write(ctx);
			break;
		case TERM:
			tcp_term(ctx);
			break;
		default:
			break;
		}

		if (ret < 0 || exit_application) {
			ctx->state = TERM;
		}
	}

	return NULL;
}

/* the main thread keeps executing this function and creates new threads here for each session */
int
handle_tcp()
{
	int ret, server_sock, sock, i, j;
	pthread_t tids[MAX_CLIENTS];
	struct context *ctx;

	/* initialize the server */
	server_sock = tcp_init_server(server_opts.address, server_opts.port);
	if (server_sock < 0) {
		ERR("Initializing TCP server failed.");
		return -1;
	}

	i = 0;
	while(!exit_application) {
		sock = accept_new_connection(server_sock);
		if (sock > 0) {
			/* new connection accepted, create new context for a new thread */
			ctx = ctx_new(sock);
			if (!ctx) {
				ERR("Creating new context failed.");
				goto cleanup;
			}

			/* create new thread for the session */
			ret = pthread_create(&tids[i++], NULL, tcp_session, ctx);
			if (ret) {
				ERR("Creating new thread failed.");
				close(sock);
				goto cleanup;
			}
		}

		usleep(100);
	}

cleanup:
	/* send interrupt signal to all threads */
	for (j = 0; j < i; j++) {
		pthread_kill(tids[j], SIGINT);
	}

	/* join the threads */
	for (j = 0; j < i; j++) {
		pthread_join(tids[j], NULL);
	}

	return ret;
}
