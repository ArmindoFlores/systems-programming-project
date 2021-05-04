#ifndef _KVS_LOCALSERVER_
#define _KVS_LOCALSERVER_

#include <sys/socket.h>

typedef struct {
    int socket;
} main_listener_ta;

typedef struct {
    int socket;
    struct sockaddr_un client;
    socklen_t length;
} conn_handler_ta;

#endif