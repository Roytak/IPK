/*
 * IPK - Project 2 (IOTA)
 * File: server.h
 * Desc: A network server for IPK Calculator Protocol header
 * Author: Roman Janota
 * Login: xjanot04
*/

#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdarg.h>

#define ERR(format, ...) fprintf(stderr, "[ERR]: " format "\n", ##__VA_ARGS__);

#define MAX_BUFFER_SIZE 2048

#define MAX_CLIENTS 128

#define SOCKET_BACKLOG 128

#define TCP_HELLO "HELLO\n"

#define TCP_BYE "BYE\n"

typedef enum {
	IP_TCP,
	IP_UDP
} protocol_type;

typedef enum {
	INIT,
	READ,
	WRITE,
	TERM
} conn_state;

struct context {
	int sock;
	conn_state state;
	char buffer[MAX_BUFFER_SIZE];
};

struct server_opts {
	const char *address;
	unsigned int port;
	protocol_type mode;
};

int handle_tcp();

int handle_udp();

#endif
