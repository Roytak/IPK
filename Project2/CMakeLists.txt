cmake_minimum_required(VERSION 2.8.12)

project(IPK2 C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -std=c11 -lpthread")

set(src
	src/server.c
	src/tcp.c
	src/parser.c
	src/udp.c)

set(header
	src/server.h
	src/parser.h)

add_executable(ipkpd ${src} ${header})
