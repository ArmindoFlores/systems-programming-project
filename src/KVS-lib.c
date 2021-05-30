#include "KVS-lib.h"
#include "common.h"
#include "ssdict.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>

static struct sockaddr_un sv_addr, cbsv_addr;
static int server = -1, callback_sock = -1;
static int connected = 0;
ssdict_t *callbacks;
pthread_mutex_t callbacks_mutex;


void get_client_secret(char *secret)
{
    srand(time(NULL) ^ getpid());
    for (int i = 0; i < CLIENTID_SIZE; i++)
        secret[i] = rand() % 256;
}

void *callback_thread(void *args)
{
    msgheader_t header;
    size_t ksize, vsize;
    char *key = NULL, *value = NULL;

    while (connected) {
        if (recvall(callback_sock, (char*) &header, sizeof(header)) != 0)
            break;
        if (header.type != NOTIFY_CALLBACK)
            continue;
        if (header.size < sizeof(ksize) + sizeof(vsize))
            continue;
        if (recvall(callback_sock, (char*) ksize, sizeof(ksize)) != 0)
            break;
        if (recvall(callback_sock, (char*) vsize, sizeof(vsize)) != 0)
            break;
        if (ksize == 0 || vsize == 0 || ksize >= MAX_KEY_SIZE || vsize >= MAX_VALUE_SIZE)
            continue;
        key = (char*) calloc(ksize+1, sizeof(char));
        value = (char*) calloc(vsize+1, sizeof(char));
        if (key == NULL || value == NULL) {
            free(key);
            free(value);
            continue;
        }
        if (recvall(callback_sock, key, ksize) != 0) 
            break;
        if (recvall(callback_sock, value, vsize) != 0)
            break;

        pthread_mutex_lock(&callbacks_mutex);
        void (*func)(char *) = (void (*)(char*)) ssdict_get(callbacks, key);
        pthread_mutex_unlock(&callbacks_mutex);
        if (func != NULL)
            func(value);

        free(key);
        free(value);
    }
    free(key);
    free(value);
}

int establish_connection (char *group_id, char *secret)
{
    if (connected)
        return ALREADY_CONNECTED;

    char csecret[CLIENTID_SIZE];
    get_client_secret(csecret);
    
    server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server == -1)
        return SOCK_ERROR;

    sv_addr.sun_family = AF_UNIX;
    strcpy(sv_addr.sun_path, SERVER_ADDR); 

    if (connect(server, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        close(server);
        return DISCONNECTED;
    }
        
    size_t gidlen = strlen(group_id), slen = strlen(secret);
    if (sendall(server, (char*)&gidlen, sizeof(gidlen)) != 0) {
        close(server);
        return DISCONNECTED;
    }
    
    if (sendall(server, group_id, gidlen) != 0) {
        close(server);
        return DISCONNECTED;
    }

    if (sendall(server, secret, slen) != 0) {
        close(server);
        return DISCONNECTED;
    }

    if (sendall(server, csecret, CLIENTID_SIZE) != 0) {
        close(server);
        return DISCONNECTED;
    }

    int status; //check if connection was accepted
    if (recvall(server, (char*)&status, sizeof(status)) != 0) {
        close(server);
        return DISCONNECTED;
    }

    switch (status) {
        case 1: //success
            break;
        case 0: // wrong groupid/secret
            close(server);
            return WRONG_LOGIN;
            break;
    }

    // Create callback related variables
    pthread_mutex_init(&callbacks_mutex, NULL);
    callbacks = ssdict_create(16);
    if (callbacks == NULL) {
        close(server);
        return MEMORY;
    }

    // Connect to callback server
    callback_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (callback_sock == -1) {
        close(server);
        return SOCK_ERROR;
    }

    cbsv_addr.sun_family = AF_UNIX;
    strcpy(cbsv_addr.sun_path, CB_SERVER_ADDR);

    if (connect(callback_sock, (struct sockaddr *)&cbsv_addr, sizeof(cbsv_addr)) < 0) {
        close(server);
        close(callback_sock);
        return DISCONNECTED;
    }

    if (csecret == NULL) {
        close(server);
        close(callback_sock);
        return MEMORY;
    }

    if (sendall(callback_sock, csecret, CLIENTID_SIZE) != 0) {
        close(server);
        close(callback_sock);
        return DISCONNECTED;
    }

    msgheader_t header;
    if (recvall(callback_sock, (char*)&header, sizeof(header)) != 0) {
        close(server);
        close(callback_sock);
        return DISCONNECTED;
    }

    if (header.type != ACK) {
        close(server);
        close(callback_sock);
        return INVALID;
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
    pthread_mutex_lock(&callbacks_mutex);
    int result = ssdict_set(callbacks, key, (char*) callback_function);
    pthread_mutex_unlock(&callbacks_mutex);
    return result;
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
        close(callback_sock);
        return SUCCESS;
    }
    return DISCONNECTED;
}
