#include "KVS-lib.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

static struct sockaddr_un sv_addr;
static int server = -1;
static int connected = 0;

int establish_connection (char *group_id, char *secret)
{
    if(connected)
        return ALREADY_CONNECTED;
    
    server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server == -1)
        return SOCK_ERROR;

    sv_addr.sun_family = AF_UNIX;
    strcpy(sv_addr.sun_path, SERVER_ADDR); 

    if (connect(server, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0)
        return DISCONNECTED;

    connected = 1;
    return SUCCESS;
}

int put_value(char *key, char *value)
{
    //! Não é final

    size_t ksize = strlen(key), vsize = strlen(value);
    msgheader_t header;
    header.type = PUT_VALUE;
    header.size = sizeof(ksize) + sizeof(vsize) + ksize + vsize;

    if (sendall(server, (char*)&header, sizeof(header)) != 0)
        return DISCONNECTED;

    if (sendall(server, (char*)&ksize, sizeof(ksize)) != 0)
        return DISCONNECTED;

    if (sendall(server, (char*)&vsize, sizeof(vsize)) != 0)
        return DISCONNECTED;

    if (sendall(server, key, ksize) != 0)
        return DISCONNECTED;

    if (sendall(server, value, ksize) != 0)
        return DISCONNECTED;

    if (recvall(server, (char*)&header, sizeof(header)) != 0)
        return DISCONNECTED;

    switch (header.type) {
        case ACK:
            return 1;
        default:
            return UNKNOWN;
    }
}

int get_value(char *key, char **value)
{
    // send(server , *key , strlen(key) , 0 );
    // read(server , **value , 1024);

}

int delete_value(char *key)
{
}

int register_callback(char *key, void (*callback_function)(char *))
{
}

int close_connection()
{
    msgheader_t header;
    header.type = DISCONNECT;
    header.size = 0;

    if (sendall(server, (char*)&header, sizeof(header)) != 0)
        return DISCONNECTED;

    close(server);
    return SUCCESS;
}
