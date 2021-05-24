#ifndef _AUTH_SERVER_
#define _AUTH_SERVER_


typedef struct {
    int socket;
    struct sockaddr_in client_addr;
    char *buffer;
    int n;
    int len;
} handle_message_ta;

#endif