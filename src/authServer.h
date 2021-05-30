#ifndef _AUTH_SERVER_
#define _AUTH_SERVER_

#include "ssdict.h"
#include <pthread.h>

typedef struct {
    int socket;
    struct sockaddr_in client_addr;
    pthread_mutex_t dict_mutex;
    ssdict_t *d;
    char *buffer;
    int n;
    int len;
} handle_message_ta;


int init_main_socket(int port);
char* generate_secret(void);
void *handle_message_thread(void *args);




#endif