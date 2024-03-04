/*
 * IPK - Project 2 (IOTA)
 * File: server.c
 * Desc: A network server for IPK Calculator Protocol
 * Author: Roman Janota
 * Login: xjanot04
*/

#define _GNU_SOURCE

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"

struct server_opts server_opts;

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
	printf("Usage: ./ipkcpd -h <host> -p <port> -m <mode>\n");
	printf("An IPK Calculator Protocol network server.\n");
	printf("Example: ./ipkcpd -h example.com -p 830 -m TCP\n");
	printf("Available options:\n");
	printf("\t--help \t\t\tDisplays this message.\n");
	printf("\t--host [-h] \t\tSpecify the address to listen on.\n");
	printf("\t--port [-p] \t\tSpecify the port to use.\n");
	printf("\t--mode [-m] \t\tSelect the mode to use, either TCP or UDP.\n");
	printf("\t--tcptest [-t] \t\tRuns a TCP server on address 127.0.0.1 on port 9999.\n");
	printf("\t--udptest [-u] \t\tRuns a UDP server on address 127.0.0.1 on port 9999.\n");
}

int
main(int argc, char *argv[])
{
	int ret = 0, opt;

	struct option options[] = {
		{"help", 	no_argument, 		NULL,	'H'},
		{"host",	required_argument,	NULL,	'h'},
		{"port",	required_argument,	NULL,	'p'},
		{"mode",	required_argument,	NULL,	'm'},
		{"tcptest",	no_argument,		NULL,	't'},
		{"udptest",	no_argument,		NULL,	'u'},
		{NULL,		0,					NULL,	0}
	};

	if (argc != 7 && argc != 2) {
		help_print();
		goto cleanup;
	}

	while ((opt = getopt_long(argc, argv, "Hh:p:m:tu", options, NULL)) != -1) {
		switch(opt) {
		case 'H':
			help_print();
			goto cleanup;
			break;
		case 'h':
			server_opts.address = optarg;
			break;
		case 'p':
			server_opts.port = atoi(optarg);
			break;
		case 'm':
			if (!strcmp(optarg, "TCP")) {
				server_opts.mode = IP_TCP;
			} else if (!strcmp(optarg, "UDP")) {
				server_opts.mode = IP_UDP;
			} else {
				ERR("Only TCP or UDP modes are allowed.");
				ret = 1;
				goto cleanup;
			}
			break;
		case 't':
			server_opts.address = "127.0.0.1";
			server_opts.port = 9999;
			server_opts.mode = IP_TCP;
			break;
		case 'u':
			server_opts.address = "127.0.0.1";
			server_opts.port = 9999;
			server_opts.mode = IP_UDP;
			break;
		default:
			ret = 1;
			break;
		}
	}

	/* set the interrupt signal handler */
	signal(SIGINT, sigint_handler);

	/* call the corresponding connection type */
	if (server_opts.mode == IP_TCP) {
		ret = handle_tcp();
	} else {
		ret = handle_udp();
	}

cleanup:
	return ret;
}
