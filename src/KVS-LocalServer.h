#ifndef _KVS_LOCALSERVER_
#define _KVS_LOCALSERVER_

#include <sys/socket.h>

typedef struct {
    int socket;
    int auth_socket;
    struct sockaddr_in sv_addr;
} main_listener_ta;

typedef struct {
    int socket;
    socklen_t length;
    int auth_socket;
    struct sockaddr_in sv_addr;
} conn_handler_ta;

#endif