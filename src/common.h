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

int recvall(int socket, char *buffer, size_t n);
int sendall(int socket, char *buffer, size_t n);

#endif