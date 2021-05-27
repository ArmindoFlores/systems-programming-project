#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#define SERVER_ADDR "/tmp/KVS-local-server"
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE 65536
#define MAX_GROUPID_SIZE 1024
#define SECRET_SIZE 16

typedef enum {
    ACK='1',
    PUT_VALUE,
    GET_VALUE,
    DEL_VALUE,
    REGISTER_CALLBACK,
    DISCONNECT,
    EINTERNAL,
    EINVALID,
    EGROUP_DELETED,
    KEY_FOUND,
    KEY_NOTFOUND,
    PING,
    CREATE_GROUP,
    DEL_GROUP,
    ERROR,
    LOGIN
} msgtype_t;

typedef struct {
    size_t size;
    msgtype_t type;
} __attribute__((packed)) msgheader_t;


/*
 * Loops until exactly n bytes are read from the socket.
 * If an error occurrs, errno is returned. Otherwise it returns 0.
 */
int recvall(int socket, char *buffer, size_t n);

/*
 * Loops until exactly n bytes are sent to the socket.
 * If an error occurrs, errno is returned. Otherwise it returns 0.
 */
int sendall(int socket, const char *buffer, size_t n);

#endif