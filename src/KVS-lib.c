#include "KVS-lib.h"
#include "common.h"
#include "list.h"
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
static ulist_t *callbacks;
static pthread_t cbt;
static pthread_mutex_t callbacks_mutex;

typedef struct {
    void (*func)(char *);
    char *key;
} func_cb;

void free_funccb(void *element)
{
    func_cb *f = (func_cb*) element;
    free(f->key);
    free(f);
}

int find_funccb(const void *element, void *arg)
{
    func_cb *f = (func_cb*) element;
    return strcmp(f->key, (char*) arg) == 0;
}

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

    printf("Started callback thread\n");
    while (connected) {
        if (recvall(callback_sock, (char*) &header, sizeof(header)) != 0)
            break;

        printf("Received callback\n");

        if (header.type != NOTIFY_CALLBACK)
            continue;
        if (header.size < sizeof(ksize) + sizeof(vsize))
            continue;
        if (recvall(callback_sock, (char*) &ksize, sizeof(ksize)) != 0)
            break;
        if (recvall(callback_sock, (char*) &vsize, sizeof(vsize)) != 0)
            break;
        if (ksize <= 0 || vsize < 0 || ksize >= MAX_KEY_SIZE || vsize >= MAX_VALUE_SIZE)
            continue;

        printf("Sizes: (%lu, %lu)\n", ksize, vsize);

        key = (char*) calloc(ksize+1, sizeof(char));
        if (vsize != 0) value = (char*) calloc(vsize+1, sizeof(char));
        if (key == NULL || (vsize != 0 && value == NULL)) {
            free(key);
            if (vsize != 0) free(value);
            continue;
        }
        if (recvall(callback_sock, key, ksize) != 0) 
            break;
        if (vsize != 0 && (recvall(callback_sock, value, vsize) != 0))
            break;

        printf("{%s, %s}\n", key, value);

        pthread_mutex_lock(&callbacks_mutex);
        func_cb *f = (func_cb*) ulist_find_element_if(callbacks, find_funccb, key);
        pthread_mutex_unlock(&callbacks_mutex);
        if (f != NULL)
            f->func(value);

        free(key);
        free(value);
        key = NULL;
        value = NULL;
    }
    free(key);
    free(value);
    return NULL;
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
    callbacks = ulist_create(free_funccb);
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

    pthread_create(&cbt, NULL, callback_thread, NULL);

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
    int result;

    if (connected) {
        size_t ksize = strlen(key);
        msgheader_t header;
        msgheader_t sv_header;
        header.type = REGISTER_CALLBACK;
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

        printf("Sent all data, receiving...\n");

        if (recvall(server, (char*)&sv_header, sizeof(header)) != 0) {    //receive sv_header
            connected = DISCONNECTED;
            return DISCONNECTED;
        }  

        if (sv_header.type != ACK)                             //Check if Key Found
            return INVALID;

        printf("Adding callback...\n");

        pthread_mutex_lock(&callbacks_mutex);
        func_cb *f = (func_cb*) malloc(sizeof(func_cb));
        f->func = callback_function;
        f->key = (char*) malloc(sizeof(char)*(ksize+1));
        strcpy(f->key, key);
        result = ulist_pushback(callbacks, f);
        pthread_mutex_unlock(&callbacks_mutex);
        if (result != 0)
            return INVALID;
        
        return SUCCESS;
    }
    return DISCONNECTED;
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
        pthread_join(cbt, NULL);
        return SUCCESS;
    }
    return DISCONNECTED;
}
