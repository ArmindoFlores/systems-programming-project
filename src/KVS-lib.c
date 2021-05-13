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
        
    size_t gidlen = strlen(group_id);
    if (sendall(server, (char*)&gidlen, sizeof(gidlen)) != 0)
        return DISCONNECTED;
    
    if (sendall(server, group_id, gidlen) != 0)
        return DISCONNECTED;
    char status=1; //check if connection was accepted

    switch(status){
        case 1: //success
            break;
        case 0: // wrong groupid/secret
            return WRONG_LOGIN;
            break;

    }


    connected = 1;
    return SUCCESS;
}

int put_value(char *key, char *value)
{
    if (connected) {
        size_t ksize = strlen(key), vsize = strlen(value);
        msgheader_t header;
        header.type = PUT_VALUE;
        header.size = sizeof(ksize) + sizeof(vsize) + ksize + vsize;

        if (sendall(server, (char*)&header, sizeof(header)) != 0) {
            connected = DISCONNECTED;
            return DISCONNECTED;
        }   

        if (sendall(server, (char*)&ksize, sizeof(ksize)) != 0) {
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (sendall(server, (char*)&vsize, sizeof(vsize)) != 0) {
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (sendall(server, key, ksize) != 0) {
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (sendall(server, value, vsize) != 0) {
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (recvall(server, (char*)&header, sizeof(header)) != 0) {
             connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        switch (header.type) {
            case ACK:
                return 1;
            default:
                return UNKNOWN;
        }
    }
    return DISCONNECTED;
}

int get_value(char *key, char **value)
{
    if (connected) {
        size_t ksize = strlen(key);
        char *sv_value;
        msgheader_t header;
        msgheader_t sv_header;
        header.type = GET_VALUE;
        header.size = sizeof(ksize) + ksize;

        if (sendall(server, (char*)&header, sizeof(header)) != 0) {      //HEADER
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if (sendall(server, (char*)&ksize, sizeof(ksize)) != 0) {            //send keysize
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if (sendall(server, key, ksize) != 0){                           //send key
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if (recvall(server, (char*)&sv_header, sizeof(header)) != 0) {    //receive sv_header
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (sv_header.type != KEY_FOUND)                             //Check if Key Found
            return NOT_FOUND;

        sv_value = (char*) malloc(sizeof(char)*(sv_header.size+1));                //allocate space for value
        if (sv_value == NULL) {
            recvall(server, NULL, sv_header.size);
            return MEMORY;
        }

        if (recvall(server, sv_value, sv_header.size) != 0) {             //receive value
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        sv_value[sv_header.size] = '\0';
        *value = sv_value;
        return SUCCESS;
    }
    return DISCONNECTED;
}

int delete_value(char *key)
{
    if (connected) {
        size_t ksize = strlen(key);
        msgheader_t header;
        msgheader_t sv_header;
        header.type = DEL_VALUE;
        header.size = sizeof(ksize) + ksize;

        if (sendall(server, (char*)&header, sizeof(header)) != 0) {       //HEADER
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if (sendall(server, (char*)&ksize, sizeof(ksize)) != 0) {         //send ksize
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if (sendall(server, key, ksize) != 0) {                           //send key
            connected = DISCONNECTED;
            return DISCONNECTED;
        } 

        if (recvall(server, (char*)&sv_header, sizeof(header)) != 0) {   //receive sv_header
            connected = DISCONNECTED;
            return DISCONNECTED;
        }

        if(sv_header.type != KEY_FOUND)                                   //Check if Key Found
            return KEY_NOTFOUND;

        return 1;
    }
    return DISCONNECTED;
}

int register_callback(char *key, void (*callback_function)(char *))
{
    
}

int close_connection()
{
    if(connected){
        msgheader_t header;
        header.type = DISCONNECT;
        header.size = 0;

        if (sendall(server, (char*)&header, sizeof(header)) != 0)
            return DISCONNECTED;

        close(server);
        return SUCCESS;
    }
    return DISCONNECTED;
}
