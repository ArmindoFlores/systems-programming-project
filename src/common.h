#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#define SERVER_ADDR "/tmp/KVS-local-server"

typedef enum {
    ACK,
    PUT_VALUE,
    GET_VALUE,
    REGISTER_CALLBACK,
    DISCONNECT,
    EINTERNAL,
    EINVALID
} msgtype_t;

typedef struct {
    size_t size;
    msgtype_t type;
} msgheader_t;

/*
 * Loops until exactly n bytes are read from the socket.
 * If an error occurrs, errno is returned. Otherwise it returns 0.
 */
int recvall(int socket, char *buffer, size_t n);

/*
 * Loops until exactly n bytes are sent to the socket.
 * If an error occurrs, errno is returned. Otherwise it returns 0.
 */
int sendall(int socket, char *buffer, size_t n);

#endif