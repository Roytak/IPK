cmake_minimum_required(VERSION 2.8.12)

project(IPK1 C)

# check the supported platform
if(NOT UNIX)
    message(FATAL_ERROR "Only *nix like systems are supported.")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wextra -std=c11")

set(src
	src/ipk.c)

set(header
	src/ipk.h)

add_executable(ipkcpc ${src} ${header})

find_program(BASH bash)
find_program(IPKPD ipkpd)

if (NOT DEFINED BASH)
    message(WARNING "bash binary not found, diasbling tests")
elseif(NOT DEFINED IPKPD)
	message(WARNING "ipkpd binary not found, diasbling tests")
else()
	enable_testing()
	add_subdirectory(tests)
endif()
