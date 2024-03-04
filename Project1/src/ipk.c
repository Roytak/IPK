/*
 * File: ipk.c
 * Desc: A network client for IPK Calculator Protocol
 * Author: Roman Janota
 * Login: xjanot04
*/

#include <assert.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ipk.h"

volatile int exit_application = 0;

static void
sigint_handler(int signum)
{
    (void) signum;
    /* notify the main loop if we should exit */
    exit_application = 1;
}

void
help_print()
{
	printf("Usage: ./ipkcpc -h <host> -p <port> -m <mode>\n");
	printf("A simple network client.\n");
	printf("Example: ./ipkcpc -h example.com -p 830 -m TCP\n");
	printf("Available options:\n");
	printf("\t--help \t\t\tDisplays this message.\n");
	printf("\t--host [-h] \t\tSpecify the host to connect to.\n");
	printf("\t--port [-p] \t\tSpecify the port to use.\n");
	printf("\t--mode [-m] \t\tSelect the mode to use, either TCP or UDP.\n");
}

/*
 * Initialization of socket and other structures needed for connection.
 */
int
init_client(const char *host, int port, protocol_type mode, int *sock, struct sockaddr_in *sin)
{
	int ret = 0;
	struct hostent *server;

	assert(sock);
	assert(sin);

	if (mode == IP_TCP) {
		*sock = socket(AF_INET, SOCK_STREAM, 0);
	} else {
		*sock = socket(AF_INET, SOCK_DGRAM, 0);
	}

	if (*sock < 0) {
		ERR("Creating a socket failed.");
		ret = 1;
		goto cleanup;
	}

	server = gethostbyname(host);
	if (!server) {
		ERR("Unable to get host \"%s\".", host);
		ret = 1;
		goto cleanup;
	}

	bzero((char *) sin, sizeof *sin);
	sin->sin_family = AF_INET;
	bcopy((char *) server->h_addr_list[0], (char *) &sin->sin_addr.s_addr, server->h_length);
	sin->sin_port = htons(port);

	if ((mode == IP_TCP) && (connect(*sock, (struct sockaddr *)sin, sizeof *sin) != 0)) {
		ERR("Couldn't connect to \"%s\".", host);
		ret = 1;
		goto cleanup;
	}

cleanup:
	return ret;
}

/*
 * Converts textual representation to the binary variant.
 */
static void
str_to_bin(char request[MAX_INPUT_SIZE])
{
	char copy[MAX_INPUT_SIZE] = {0};
	unsigned char len;

	memcpy(copy, request, MAX_INPUT_SIZE);

	memset(request, 0, MAX_INPUT_SIZE);
	if (strlen(copy) > 255) {
		len = 255;
	} else {
		len = strlen(copy);
	}

	request[1] = len;
	memcpy(request + 2, copy, len);
}

/*
 * Converts textual representation to the binary variant.
 * Returns 0 if the response is ok, 1 if an error occurred.
 */
static int
bin_to_str(char resp[MAX_INPUT_SIZE])
{
	int ret = 0;

	char copy[MAX_INPUT_SIZE] = {0};

	memcpy(copy, resp, MAX_INPUT_SIZE);

	if (resp[1]) {
		ret = 1;
	}

	memset(resp, 0, MAX_INPUT_SIZE);
	memcpy(resp, copy + 3, copy[2]);

	return ret;
}

int
main(int argc, char *argv[])
{
	int ret = 0, opt = 0, port = 0, sock = -1, mode = 0;
	const char *host = NULL;
	struct sockaddr_in sin, *sin_p;
	char send_buf[MAX_INPUT_SIZE] = {0};
	char recv_buf[MAX_INPUT_SIZE] = {0};
	ssize_t sent, received, to_send;
	socklen_t addrlen;

	struct option options[] = {
		{"help", 	no_argument, 		NULL,	'H'},
		{"host",	required_argument,	NULL,	'h'},
		{"port",	required_argument,	NULL,	'p'},
		{"mode",	required_argument,	NULL,	'm'},
		{NULL,		0,					NULL,	0}
	};

	if (argc != 7) {
		help_print();
		goto cleanup;
	}

	/* parse args */
	while ((opt = getopt_long(argc, argv, "Hh:p:m:", options, NULL)) != -1) {
		switch(opt) {
		case 'H':
			help_print();
			break;
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			if (port > MAX_PORT) {
				ERR("Given port is too high.");
				ret = 1;
				goto cleanup;
			}
			break;
		case 'm':
			if (!strcmp(optarg, "TCP")) {
				mode = IP_TCP;
			} else if (!strcmp(optarg, "UDP")) {
				mode = IP_UDP;
			} else {
				ERR("Only TCP or UDP modes are allowed.");
				ret = 1;
				goto cleanup;
			}
			break;
		default:
			ret = 1;
			break;
		}
	}

	/* initialize the socket for connection */
	if (init_client(host, port, mode, &sock, &sin)) {
		ERR("Initializing client failed.");
		ret = 1;
		goto cleanup;
	}

	/* set the signal handler */
	signal(SIGINT, sigint_handler);

	if (mode == IP_TCP) {
		sin_p = NULL;
	} else {
		sin_p = &sin;
	}
	addrlen = sizeof sin;

	while (!exit_application && (fgets(send_buf, MAX_INPUT_SIZE, stdin) != NULL)) {
		if (send_buf[0] == '\n') {
			continue;
		}

		if (mode == IP_TCP) {
			to_send = strlen(send_buf);
		} else {
			/* sending 2 extra bytes, that is opcode and payload length */
			to_send = strlen(send_buf) + 2;
		}

		if (mode == IP_UDP) {
			/* convert to the correct format */
			str_to_bin(send_buf);
		}

		sent = sendto(sock, send_buf, to_send, 0, (const struct sockaddr *)sin_p, addrlen);
		if (sent != to_send) {
			ERR("Error sending a message.");
			ret = 1;
			goto cleanup;
		}

		received = recvfrom(sock, recv_buf, MAX_INPUT_SIZE, 0, (struct sockaddr *)sin_p, &addrlen);
		if ((received == 0) && (mode == IP_TCP)) {
			/* connection terminated, send bye */
			sent = send(sock, "BYE", strlen("BYE"), 0);
			if (sent != (int)strlen("BYE")) {
				ERR("Error receiving a message.");
				ret = 1;
			}
			goto cleanup;
		} else if (received < 0) {
			ERR("Receiving message failed.");
			ret = 1;
			goto cleanup;
		}

		if (mode == IP_UDP) {
			/* convert response back to readable format */
			if (!bin_to_str(recv_buf)) {
				printf("OK:%s\n", recv_buf);
			} else {
				printf("ERR:%s\n", recv_buf);
			}
		} else {
			printf("%s", recv_buf);
		}

		memset(send_buf, 0, MAX_INPUT_SIZE);
		memset(recv_buf, 0, MAX_INPUT_SIZE);
	}

	if (exit_application && (mode == IP_TCP)) {
		/* if the client got C-c, send a bye message and wait for the response before terminating */
		sent = send(sock, "BYE\n", strlen("BYE\n"), 0);
		if (sent != (int)strlen("BYE\n")) {
			ERR("Error sending a BYE message.");
			ret = 1;
			goto cleanup;
		}

		memset(recv_buf, 0, MAX_INPUT_SIZE);
		received = recv(sock, recv_buf, MAX_INPUT_SIZE, 0);
		if (strcmp(recv_buf, "BYE\n")) {
			ERR("Error receiving a BYE message.");
			ret = 1;
			goto cleanup;
		}
		printf("%s", recv_buf);
	}

cleanup:
	if (sock >= 0) {
		close(sock);
	}
	return ret;
}
