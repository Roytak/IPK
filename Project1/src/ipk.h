/*
 * File: ipk.h
 * Desc: Header file for a network client
 * Author: Roman Janota
 * Login: xjanot04
*/

#ifndef _IPK_H_
#define _IPK_H_

#include <stdarg.h>

#define ERR(format, ...) fprintf(stderr, "[ERR]: " format "\n", ##__VA_ARGS__);

#define MAX_INPUT_SIZE 2048

#define MAX_PORT 65535

typedef enum {
	IP_TCP,
	IP_UDP
} protocol_type;

#endif
