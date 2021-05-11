#define  _GNU_SOURCE // Otherwise we get a warning about getline()
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include "KVS-LocalServer.h"
#include "common.h"
#include "list.h"
#include "ssdict.h"
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE 65536


typedef struct {
    char *groupid;
    ssdict_t *d;
    pthread_mutex_t mutex;
} glelement_t;

static struct gl {
    ulist_t *list;
    pthread_mutex_t mutex;
} grouplist;


void free_glelement(void *arg)
{
    glelement_t *element = (glelement_t*) arg;
    ssdict_free(element->d);
    free(element->groupid);
    pthread_mutex_destroy(&element->mutex);
    free(element);
}

int find_glelement(const void *element, void* arg)
{
    return strcmp(((glelement_t*)element)->groupid, (char*) arg) == 0;
}

int init_main_socket(char *sock_path)
{
    int s;
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error while creating the socket\n");
        exit(EXIT_FAILURE);
    }
    remove(addr.sun_path);  // Delete the socket file so subsequent runs don't error out

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "Error while binding the address\n");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 4) == -1) {
        fprintf(stderr, "Error while trying to listen for connections\n");
        exit(EXIT_FAILURE);
    }

    return s;
}

int msg_put_value(int socket, msgheader_t *h, char *groupid) 
{
    msgheader_t msg;
    msg.size = 0;
    msg.type = ACK;
    char *key, *value;
    size_t ksize, vsize;

    // Make sure the message size makes sense (has to contain at least ksize and vsize)
    if (h->size < sizeof(ksize) + sizeof(vsize))
        return 0;

    if (recvall(socket, (char*)&ksize, sizeof(ksize)) != 0)
        return 0;

    if (recvall(socket, (char*)&vsize, sizeof(vsize)) != 0)
        return 0;

    printf("%lu, %lu\n", ksize, vsize);

    // Make sure the client is sending valid sizes
    if (ksize == 0 || vsize == 0 || ksize >= MAX_KEY_SIZE || vsize >= MAX_VALUE_SIZE)
        return 0;

    // Allocate memory for the KV pair
    key = (char*) calloc(ksize+1, sizeof(char));
    value = (char*) calloc(vsize+1, sizeof(char));

    if (key == NULL || value == NULL) {
        // Read all data, but ignore it and exit
        recvall(socket, NULL, ksize);
        recvall(socket, NULL, vsize);
        msg.type = EINTERNAL;
        sendall(socket, (char*)&msg, sizeof(msg));
        return 1;
    }

    // Attempt to receive KV pair
    if (recvall(socket, key, ksize) != 0) {
        free(key);
        free(value);
        return 0;
    }

    if (recvall(socket, value, vsize) != 0) {
        free(key);
        free(value);
        return 0;
    }

    //TODO: Add the key-value pair to the stored key-value pairs
    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t*) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) { 
        // Group was deleted, notify client and close the connection
        free(key);
        free(value);
        msg.type = EGROUP_DELETED;
        sendall(socket, (char*)&msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }
    if (ssdict_set(group->d, key, value) != 0) {
        // Internal error occurred (keep going?)
        free(key);
        free(value);
        msg.type = EINTERNAL;
        sendall(socket, (char*)&msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 1;
    }

    printf("[%d] Stored the KV pair ('%s', '%s')\n", socket, key, value);
    pthread_mutex_unlock(&grouplist.mutex);

    free(key);
    free(value);

    // Notify client that the KV pair has been stored or that an error has occurred
    if (sendall(socket, (char*)&msg, sizeof(msg)) != 0)
        return 0;

    return 1;
}

int msg_get_value() 
{
    return 0;
}

int msg_register_callback() 
{
    return 0;
}

void *connection_handler_thread(void *args)
{
    int running = 1;
    conn_handler_ta *ta = (conn_handler_ta*) args;
    char *groupid = NULL;
    size_t gidlen;

    if (recvall(ta->socket, (char*)&gidlen, sizeof(gidlen)) != 0) {
        close(ta->socket);
        free(ta);
        printf("Disconnected\n");
        return NULL;
    }

    groupid = (char*) malloc(sizeof(char)*(gidlen + 1));
    if (groupid == NULL) {
        close(ta->socket);
        free(ta);
        printf("Disconnected (internal error)\n");
        return NULL;
    }

    if (recvall(ta->socket, groupid, gidlen) != 0) {
        running = 0;
    }
    else {
        groupid[gidlen] = '\0';
        pthread_mutex_lock(&grouplist.mutex);
        glelement_t *group = (glelement_t*) ulist_find_element_if(grouplist.list, find_glelement, groupid);
        if (group == NULL) { // No information is stored for this group yet
            group = (glelement_t*) malloc(sizeof(glelement_t));
            if (group == NULL) {
                free(groupid);
                close(ta->socket);
                free(ta);
                printf("Disconnected (internal error)\n");
                return NULL;
            }
            group->d = ssdict_create(16);
            group->groupid = (char*) malloc(sizeof(char)*(gidlen+1));
            if (group->d == NULL || group->groupid == NULL) {
                free(group->d);
                free(group->groupid);
                free(groupid);
                close(ta->socket);
                free(ta);
                printf("Disconnected (internal error)\n");
                return NULL;
            }
            strcpy(group->groupid, groupid);
            group->groupid[gidlen] = '\0';
            pthread_mutex_init(&group->mutex, NULL);
            if (ulist_pushback(grouplist.list, group) != 0) {
                free(groupid);
                free_glelement(group);
                close(ta->socket);
                free(ta);
                printf("Disconnected (internal error)\n");
                return NULL;
            }
        }
        pthread_mutex_unlock(&grouplist.mutex);
    }

    msgheader_t header;
    while (running) {
        if (recvall(ta->socket, (char*)&header, sizeof(header)) != 0) 
            break;
        switch (header.type) {
            case PUT_VALUE:
                running = msg_put_value(ta->socket, &header, groupid);
                break;
            case GET_VALUE:
                running = msg_get_value();
                break;
            case REGISTER_CALLBACK:
                running = msg_register_callback();
                break;
            case DISCONNECT:
                running = 0;
                break;
            default:
                running = 0;
                break;
        }
    }

    free(groupid);
    close(ta->socket);
    free(ta);
    printf("Disconnected\n");
    return NULL;
}

void *main_listener_thread(void *args)
{
    main_listener_ta *ta = (main_listener_ta*) args;

    struct sockaddr_un incoming;
    socklen_t length = sizeof(incoming);
    while (1) {
        // Listen for new connections
        int new_socket = accept(ta->socket, (struct sockaddr*)&incoming, &length);
        printf("Got a new connection!\n");

        // Start up new thread responsible for this connection
        conn_handler_ta *args = (conn_handler_ta*) malloc(sizeof(conn_handler_ta));
        if (args == NULL) {
            close(new_socket);
            printf("Error allocating memory!\n");
            continue;
        }
        args->socket = new_socket;
        args->client = incoming;
        args->length = length;
        pthread_t child;
        if (pthread_create(&child, NULL, connection_handler_thread, args) != 0) {
            fprintf(stderr, "Error while creating thread\n");
            free(args);
            continue;
        }
        if (pthread_detach(child) != 0) {
            fprintf(stderr, "Error while detaching thread\n");
            continue;
        }
    }
}

int main() 
{
    // Create list of groups and dicts
    grouplist.list = ulist_create(free_glelement);
    pthread_mutex_init(&grouplist.mutex, NULL);

    // Create local socket
    int s = init_main_socket(SERVER_ADDR);

    // Start up new thread to handle connections
    main_listener_ta args = { s };
    pthread_t main_listener;
    if (pthread_create(&main_listener, NULL, main_listener_thread, &args) != 0) {
        fprintf(stderr, "Error while creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(main_listener);

    // Process user commands
    char *line = NULL;
    size_t size = 0;
    while (1) {
        printf(">>> ");
        getline(&line, &size, stdin);

        if (strncmp(line, "exit", 4) == 0)
            break;
    }

    free(line);
    pthread_mutex_destroy(&grouplist.mutex);
    ulist_free(grouplist.list);
}
