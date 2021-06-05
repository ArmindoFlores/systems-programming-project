#define _GNU_SOURCE // Otherwise we get a warning about getline()
#include "KVS-LocalServer.h"
#include "common.h"
#include "list.h"
#include "ssdict.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define XSTR(a) STR(a)
#define STR(a) #a

#define TIMEOUT 5 * CLOCKS_PER_SEC

typedef struct {
    char *groupid;
    ssdict_t *d;
    ulist_t *callbacks;
    pthread_mutex_t mutex;
} glelement_t;

typedef struct {
    int socket;
    char clientid[CLIENTID_SIZE];
    pthread_mutex_t mutex;
} cbelement_t;

typedef struct {
    int pid;
    time_t connect, close;
    int closed;
} ilelement_t;

typedef struct {
    ulist_t *list;
    char *key;
} cbclient_t;

static struct cbl {
    ulist_t *list;
    pthread_mutex_t mutex;
} callbacklist;

static struct infol {
    ulist_t *list;
    pthread_mutex_t mutex;
} infolist;

static struct gl {
    ulist_t *list;
    pthread_mutex_t mutex;
} grouplist;

typedef struct {
    char *key, *value;
} kvpair;

static int main_socket, callback_socket, auth_socket;
static struct sockaddr_in sv_addr;


void print_ilelement(const void *e)
{
    char time[256] = "";
    struct tm *time_s;
    ilelement_t *info = (ilelement_t*) e;
    
    time_s = localtime(&info->connect);
    strftime(time, sizeof(time), "%x - %T", time_s);
    printf("<PID=%d (CONNECTED AT %s", info->pid, time);
    if (info->closed) {
        time_s = localtime(&info->close);
        strftime(time, sizeof(time), "%x - %T", time_s);
        printf(" | DISCONNECTED AT %s", time);
    }
    printf(")>");
}

int find_client(const void *element, void *arg) { return strcmp(((cbclient_t *) element)->key, (char *) arg) == 0; }

int find_ilelement(const void *element, void *arg) {
    ilelement_t *info = (ilelement_t*) element;
    int pid = *((int*) arg);
    return info->pid == pid && info->closed == 0;
}

void free_glelement(void *arg)
{
    glelement_t *element = (glelement_t *) arg;
    ulist_free(element->callbacks);
    ssdict_free(element->d);
    free(element->groupid);
    pthread_mutex_destroy(&element->mutex);
    free(element);
}

void free_cbelement(void *arg)
{
    cbelement_t *element = (cbelement_t *) arg;
    close(element->socket);
    pthread_mutex_destroy(&element->mutex);
    free(element);
}

void free_cbclient(void *arg)
{
    cbclient_t *element = (cbclient_t *) arg;
    ulist_free(element->list);
    free(element->key);
    free(element);
}

int find_glelement(const void *element, void *arg) { return strcmp(((glelement_t *) element)->groupid, (char *) arg) == 0; }

int find_addr(const void *element, void *arg) { return memcmp(((cbelement_t *) element)->clientid, (char *) arg, CLIENTID_SIZE) == 0; }

int find_cidstr(const void *element, void *arg) { return memcmp((const char *) element, (char *) arg, CLIENTID_SIZE) == 0; }

void get_client_secret(char *secret, int pid)
{
    srand(time(NULL) ^ pid);
    for (int i = 0; i < CLIENTID_SIZE; i++) secret[i] = rand() % 256;
}

int init_AF_UNIX_socket(char *sock_path)
{
    int s;
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, sock_path);

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error while creating the socket\n");
        exit(EXIT_FAILURE);
    }
    remove(addr.sun_path); // Delete the socket file so subsequent runs don't error out

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        fprintf(stderr, "Error while binding the address\n");
        exit(EXIT_FAILURE);
    }

    if (listen(s, 4) == -1) {
        fprintf(stderr, "Error while trying to listen for connections\n");
        exit(EXIT_FAILURE);
    }

    return s;
}

void remove_client(void *element, void *args)
{
    cbclient_t *e = (cbclient_t*) element;
    char *clientid = (char*) args;
    ulist_remove_if(e->list, find_cidstr, clientid);
}

void send_callback_msg(void *element, void *args)
{
    kvpair *pair = (kvpair *) args;
    char *key = pair->key, *value = pair->value, *clientid = (char *) element;
    size_t ksize = strlen(key), vsize = pair->value == NULL ? 0 : strlen(value);

    cbelement_t *e = (cbelement_t *) ulist_find_element_if(callbacklist.list, find_addr, clientid);
    if (e == NULL) return;

    msgheader_t header;
    header.type = NOTIFY_CALLBACK;
    header.size = sizeof(ksize) + sizeof(vsize) + ksize + vsize;

    pthread_mutex_lock(&e->mutex);
    sendall(e->socket, (char *) &header, sizeof(header));
    sendall(e->socket, (char *) &ksize, sizeof(ksize));
    sendall(e->socket, (char *) &vsize, sizeof(vsize));
    sendall(e->socket, key, ksize);
    if (vsize != 0) sendall(e->socket, value, vsize);
    pthread_mutex_unlock(&e->mutex);
}

int notify_callback(char *key, char *value, char *groupid)
{
    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) {
        pthread_mutex_unlock(&grouplist.mutex);
        return -1;
    }
    cbclient_t *cbc = (cbclient_t *) ulist_find_element_if(group->callbacks, find_client, key);
    if (cbc == NULL) {
        pthread_mutex_unlock(&grouplist.mutex);
        return -1;
    }
    ulist_t *clients = cbc->list;

    kvpair pair = {key, value};
    pthread_mutex_unlock(&grouplist.mutex);

    ulist_exec(clients, send_callback_msg, &pair);
    return 0;
}

int msg_put_value(int socket, msgheader_t *h, char *groupid)
{
    msgheader_t msg;
    msg.size = 0;
    msg.type = ACK;
    char *key, *value;
    size_t ksize, vsize;

    // Make sure the message size makes sense (has to contain at least ksize and vsize)
    if (h->size < sizeof(ksize) + sizeof(vsize)) return 0;

    if (recvall(socket, (char *) &ksize, sizeof(ksize)) != 0) return 0;

    if (recvall(socket, (char *) &vsize, sizeof(vsize)) != 0) return 0;

    // Make sure the client is sending valid sizes
    if (ksize == 0 || vsize == 0 || ksize >= MAX_KEY_SIZE || vsize >= MAX_VALUE_SIZE) return 0;

    // Allocate memory for the KV pair
    key = (char *) calloc(ksize + 1, sizeof(char));
    value = (char *) calloc(vsize + 1, sizeof(char));

    if (key == NULL || value == NULL) {
        // Read all data, but ignore it and exit
        free(key);
        free(value);
        recvall(socket, NULL, ksize);
        recvall(socket, NULL, vsize);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
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

    // Add KV pair to the group's dictionary
    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) {
        // Group was deleted, notify client and close the connection
        free(key);
        free(value);
        msg.type = EGROUP_DELETED;
        sendall(socket, (char *) &msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }
    if (ssdict_set(group->d, key, value) != 0) {
        // Internal error occurred (keep going?)
        free(key);
        free(value);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 1;
    }

    pthread_mutex_unlock(&grouplist.mutex);

    // Notify all clients that registered a callback
    notify_callback(key, value, groupid);

    free(key);
    free(value);

    // Notify client that the KV pair has been stored or that an error has occurred
    if (sendall(socket, (char *) &msg, sizeof(msg)) != 0) return 0;

    return 1;
}

int msg_get_value(int socket, msgheader_t *h, char *groupid)
{
    msgheader_t msg;
    msg.size = 0;
    msg.type = ACK;
    char *key;
    size_t ksize;

    // Make sure the message size makes sense (has to contain at least ksize)
    if (h->size < sizeof(ksize)) return 0;

    if (recvall(socket, (char *) &ksize, sizeof(ksize)) != 0) return 0;

    if (h->size != ksize + sizeof(ksize)) return 0;

    key = (char *) calloc(ksize + 1, sizeof(char));
    if (key == NULL) {
        // Read all data, but ignore it and exit
        recvall(socket, NULL, ksize);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
        free(key);
        return 1;
    }

    // Attempt to receive the key
    if (recvall(socket, key, ksize) != 0) {
        free(key);
        return 0;
    }
    key[ksize] = '\0';

    // Get the value corresponding to the key in the group's dictionary
    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) {
        // Group was deleted, notify client and close the connection
        free(key);
        msg.type = EGROUP_DELETED;
        sendall(socket, (char *) &msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }
    const char *value = ssdict_get(group->d, key);
    pthread_mutex_unlock(&grouplist.mutex);

    msg.type = value == NULL ? KEY_NOTFOUND : KEY_FOUND;
    msg.size = value == NULL ? 0 : strlen(value);

    free(key);

    // Send response
    if (sendall(socket, (char *) &msg, sizeof(msg)) != 0) return 0;
    if (msg.type == KEY_FOUND) {
        if (sendall(socket, value, msg.size) != 0) return 0;
    }
    return 1;
}

int msg_delete_value(int socket, msgheader_t *h, char *groupid)
{
    msgheader_t msg;
    msg.size = 0;
    msg.type = KEY_FOUND;
    char *key;
    size_t ksize;

    // Make sure the message size makes sense (has to contain at least ksize and vsize)
    if (h->size < sizeof(ksize)) return 0;

    if (recvall(socket, (char *) &ksize, sizeof(ksize)) != 0) return 0;

    // Make sure the client is sending valid sizes
    if (ksize == 0 || ksize >= MAX_KEY_SIZE) return 0;

    // Allocate memory for the KV pair
    key = (char *) calloc(ksize + 1, sizeof(char));

    if (key == NULL) {
        // Read all data, but ignore it and exit
        free(key);
        recvall(socket, NULL, ksize);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
        return 1;
    }

    // Attempt to receive KV pair
    if (recvall(socket, key, ksize) != 0) {
        free(key);
        return 0;
    }

    // Add KV pair to the group's dictionary
    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) {
        // Group was deleted, notify client and close the connection
        free(key);
        msg.type = EGROUP_DELETED;
        sendall(socket, (char *) &msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }
    if (ssdict_set(group->d, key, NULL) != 0) {
        // Internal error occurred (keep going?)
        free(key);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 1;
    }

    pthread_mutex_unlock(&grouplist.mutex);

    // Notify all clients that registered a callback
    notify_callback(key, NULL, groupid);

    free(key);

    // Notify client that the KV pair has been stored or that an error has occurred
    if (sendall(socket, (char *) &msg, sizeof(msg)) != 0) return 0;

    return 1;
}

int msg_register_callback(int socket, msgheader_t *h, char *groupid, char *clientid)
{
    msgheader_t msg;
    msg.size = 0;
    msg.type = ACK;
    char *key;
    size_t ksize;

    // Make sure the message size makes sense (has to contain at least ksize)
    if (h->size < sizeof(ksize)) return 0;

    if (recvall(socket, (char *) &ksize, sizeof(ksize)) != 0) return 0;

    if (h->size != ksize + sizeof(ksize)) return 0;

    key = (char *) calloc(ksize + 1, sizeof(char));
    if (key == NULL) {
        // Read all data, but ignore it and exit
        recvall(socket, NULL, ksize);
        msg.type = EINTERNAL;
        sendall(socket, (char *) &msg, sizeof(msg));
        free(key);
        return 1;
    }

    // Attempt to receive the key
    if (recvall(socket, key, ksize) != 0) {
        free(key);
        return 0;
    }
    key[ksize] = '\0';

    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *gel = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (gel == NULL) {
        free(key);
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }

    msgtype_t t = ACK;

    if (gel != NULL) {
        cbclient_t *cbclient = (cbclient_t *) ulist_find_element_if(gel->callbacks, find_client, key);
        if (cbclient == NULL) {
            ulist_t *client = ulist_create(free);
            cbclient = (cbclient_t *) malloc(sizeof(cbclient_t));
            cbclient->key = (char *) malloc(sizeof(char) * (ksize + 1));
            memcpy(cbclient->key, key, ksize);
            cbclient->key[ksize] = '\0';
            cbclient->list = client;
            ulist_pushback(gel->callbacks, cbclient);
        }
        if (cbclient != NULL) {
            free(key);
            if (ulist_find_element_if(cbclient->list, find_cidstr, clientid) != NULL)
                t = ERROR;
            else {
                char *cid = (char *) malloc(CLIENTID_SIZE);
                if (cid == NULL)
                    t = ERROR;
                else {
                    memcpy(cid, clientid, CLIENTID_SIZE);
                    ulist_pushback(cbclient->list, cid);
                }
                msg.type = t;
                msg.size = 0;
                if (sendall(socket, (char *) &msg, sizeof(msg)) != 0) return 0;
            }
            pthread_mutex_unlock(&grouplist.mutex);
        } else {
            free(key);
            pthread_mutex_unlock(&grouplist.mutex);
        }
    } else {
        free(key);
        pthread_mutex_unlock(&grouplist.mutex);
    }

    return 1;
}

int delete_group(char *groupid, int as, struct sockaddr_in sv_addr)
{
    int result, n = -1;
    char buffer[1024];
    char *message;
    size_t gidlen = strlen(groupid);

    groupid[gidlen] = '\0';
    message = (char *) malloc(sizeof(char) *( gidlen + 1+1));
    message[0] = DEL_GROUP;
    strncpy(message + 1, groupid, gidlen);
    message[1 + gidlen] = '\0';

    pthread_mutex_lock(&grouplist.mutex);
    result = ulist_remove_if(grouplist.list, find_glelement, groupid);
    pthread_mutex_unlock(&grouplist.mutex);

    if (!result) {
        sendto(as, (const char *) message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
        free(message);
        socklen_t len = sizeof(sv_addr);
        time_t before = clock();
        while (clock() - before < TIMEOUT && n <= 0) n = recvfrom(as, (char *) buffer, 1024, MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
        if (buffer[0] == ERROR) return -2;
        if (n <= 0) return -1;
    }
    return !result;
}

int create_group(char *groupid, int as, struct sockaddr_in sv_addr)
{
    size_t gidlen = strlen(groupid);

    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) { // No information is stored for this group yet
        char buffer[1024] = "";
        char *message = (char *) malloc(sizeof(char) * (gidlen + 1));
        message[0] = CREATE_GROUP;
        memcpy(message + 1, groupid, gidlen);

        sendto(as, (const char *) message, gidlen + 1, MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
        int n = -1;
        socklen_t len = sizeof(sv_addr);
        time_t before = clock();
        while (clock() - before < TIMEOUT && n <= 0) n = recvfrom(as, (char *) buffer, 1024, MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
        if (buffer[0] == ERROR) {
            free(message);
            pthread_mutex_unlock(&grouplist.mutex);
            return -2;
        }
        if (n <= 0) {
            free(message);
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        printf("The secret for group '%s' is '%s'\n", groupid, buffer + 1);
        free(message);

        group = (glelement_t *) malloc(sizeof(glelement_t));
        if (group == NULL) {
            free(groupid);
            fprintf(stderr, "Memory error (internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }
        pthread_mutex_init(&group->mutex, NULL);

        group->d = ssdict_create(16);
        group->groupid = (char *) malloc(sizeof(char) * (gidlen + 1));
        if (group->d == NULL || group->groupid == NULL) {
            free(group->d);
            free(group->groupid);
            free(groupid);
            fprintf(stderr, "Memory error (internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        strcpy(group->groupid, groupid);
        group->groupid[gidlen] = '\0';

        group->callbacks = ulist_create(free_cbclient);
        if (group->callbacks == NULL) {
            free(group->d);
            free(group->groupid);
            free(groupid);
            fprintf(stderr, "Memory error (internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        if (ulist_pushback(grouplist.list, group) != 0) {
            free(groupid);
            free_glelement(group);
            fprintf(stderr, "Memory error (internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

    } else {
        pthread_mutex_unlock(&grouplist.mutex);
        return -2;
    }
    pthread_mutex_unlock(&grouplist.mutex);
    return 1;
}

int login_auth(char *gid, char *secret, int as, struct sockaddr_in sv_addr)
{
    char *message = (char *) malloc(strlen(gid) + SECRET_SIZE + 1 + 1);
    message[0] = LOGIN;
    message[strlen(gid) + SECRET_SIZE + 1] = '\0';
    memcpy(message + 1, secret, SECRET_SIZE);
    memcpy(message + 1 + SECRET_SIZE, gid, strlen(gid));
    printf("Sending login attempt (%s)\n", message+1);
    sendto(as, (const char *) message, strlen(message + 1) + 1, MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
    char buffer[1024];
    int n = -1;
    socklen_t len = sizeof(sv_addr);
    time_t before = clock();
    buffer[0] = ERROR;
    while (clock() - before < TIMEOUT && n < 0) n = recvfrom(as, (char *) buffer, 1024, MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
    if (buffer[0] == ERROR) return -2;
    if (n < 0) return -1;

    free(message);

    return 1;
}

void *connection_handler_thread(void *args)
{
    int running = 1;
    conn_handler_ta *ta = (conn_handler_ta *) args;
    char *groupid = NULL, secret[SECRET_SIZE] = "", clientid[CLIENTID_SIZE] = "";
    size_t gidlen, pidlen;
    char pidstr[MAX_PID_SIZE + 1];
    int pid;
    ilelement_t *info;

    if (recvall(ta->socket, (char *) &gidlen, sizeof(gidlen)) != 0 || gidlen > MAX_GROUPID_SIZE) {
        close(ta->socket);
        free(ta);
        return NULL;
    }

    if (recvall(ta->socket, (char *) &pidlen, sizeof(pidlen)) != 0 || pidlen > MAX_PID_SIZE) {
        close(ta->socket);
        free(ta);
        return NULL;
    }

    groupid = (char *) calloc((gidlen + 1), sizeof(char));
    if (groupid == NULL) {
        close(ta->socket);
        free(ta);
        printf("Disconnected (internal error)\n");
        return NULL;
    }

    if (recvall(ta->socket, groupid, gidlen) != 0 || 
        recvall(ta->socket, pidstr, pidlen) != 0 ||
        recvall(ta->socket, secret, SECRET_SIZE) != 0)
        running = 0;

    pidstr[pidlen] = '\0';
    groupid[gidlen] = '\0';

    pid = atoi(pidstr);
    get_client_secret(clientid, pid);

    if (running && ulist_find_element_if(grouplist.list, find_glelement, groupid) == NULL) { // check if group exists
        running = 0;
        // dont accept
    } 
    else if (login_auth(groupid, secret, auth_socket, sv_addr) != 1) {
        printf("Login failed for client with PID '%d'\n", pid);
        running = 0;
    }

    msgheader_t response;
    if (!running) {
        response.size = 0;
        response.type = EINVALID;
    }
    else {
        response.size = CLIENTID_SIZE;
        response.type = ACK;
    }
    if (sendall(ta->socket, (char *) &response, sizeof(response)) != 0) running = 0;
    if (running && sendall(ta->socket, clientid, CLIENTID_SIZE) != 0) running = 0;

    info = (ilelement_t*) malloc(sizeof(ilelement_t));
    info->closed = 0;
    info->close = 0;
    info->connect = time(NULL);
    info->pid = pid;

    pthread_mutex_lock(&infolist.mutex);
    int result = ulist_pushback(infolist.list, info);
    pthread_mutex_unlock(&infolist.mutex);
    if (result != 0) running = 0;


    if (running) printf("Login sucessful from client with PID '%d'\n", pid);
    msgheader_t header;
    while (running) {
        if (recvall(ta->socket, (char *) &header, sizeof(header)) != 0) break;
        switch (header.type) {
        case PUT_VALUE:
            running = msg_put_value(ta->socket, &header, groupid);
            break;
        case GET_VALUE:
            running = msg_get_value(ta->socket, &header, groupid);
            break;
        case DEL_VALUE:
            running = msg_delete_value(ta->socket, &header, groupid);
            break;
        case REGISTER_CALLBACK:
            running = msg_register_callback(ta->socket, &header, groupid, clientid);
            break;
        case DISCONNECT:
            running = 0;
            break;
        default:
            running = 0;
            break;
        }
    }

    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *gel = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (gel != NULL)
        ulist_exec(gel->callbacks, remove_client, clientid);
    pthread_mutex_unlock(&grouplist.mutex);

    pthread_mutex_lock(&infolist.mutex);
    info = (ilelement_t*) ulist_find_element_if(infolist.list, find_ilelement, &pid);
    if (info != NULL) {
        info->closed = 1;
        info->close = time(NULL);
    }
    pthread_mutex_unlock(&infolist.mutex);

    free(groupid);
    close(ta->socket);
    free(ta);
    printf("Lost connection from client with PID '%d'\n", pid);
    return NULL;
}

void *callback_listener_thread(void *args)
{
    struct sockaddr_un incoming;
    socklen_t length = sizeof(incoming);
    while (1) {
        // Listen for new connections
        int new_socket = accept(callback_socket, (struct sockaddr *) &incoming, &length);
        cbelement_t *e = (cbelement_t *) malloc(sizeof(cbelement_t));
        if (e == NULL) {
            free(e);
            continue;
        }

        if (recvall(new_socket, e->clientid, CLIENTID_SIZE) != 0) {
            free(e);
            continue;
        }

        e->socket = new_socket;
        pthread_mutex_init(&e->mutex, NULL);

        pthread_mutex_lock(&callbacklist.mutex);
        cbelement_t *found = (cbelement_t *) ulist_find_element_if(callbacklist.list, find_addr, e->clientid);
        if (!found)
            ulist_pushback(callbacklist.list, e);
        pthread_mutex_unlock(&callbacklist.mutex);

        msgheader_t msg;
        msg.size = 0;
        msg.type = found != NULL ? ERROR : ACK;
        sendall(new_socket, (char *) &msg, sizeof(msg));
    }
}

void *main_listener_thread(void *args)
{
    struct sockaddr_un incoming;
    socklen_t length = sizeof(incoming);
    while (1) {
        // Listen for new connections
        int new_socket = accept(main_socket, (struct sockaddr *) &incoming, &length);
        printf("Got a new connection!\n");

        // Start up new thread responsible for this connection
        conn_handler_ta *args = (conn_handler_ta *) malloc(sizeof(conn_handler_ta));
        if (args == NULL) {
            close(new_socket);
            printf("Error allocating memory!\n");
            continue;
        }
        args->socket = new_socket;
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

char get_option(char *arg)
{
    if (strcmp(arg, "exit") == 0) return 0;
    if (strcmp(arg, "show") == 0) return 1;
    if (strcmp(arg, "create") == 0) return 2;
    if (strcmp(arg, "delete") == 0) return 3;
    if (strcmp(arg, "status") == 0) return 4;
    return -1;
}

int init_auth_socket(char **argv, struct sockaddr_in *sv_addr)
{

    int s, n = -1;
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error while creating the socket\n");
        exit(EXIT_FAILURE);
    }
    sv_addr->sin_family = AF_INET;
    printf("AuthServer Address=%s Port=%s\n", argv[1], argv[2]);
    sv_addr->sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &sv_addr->sin_addr);

    char *message = (char *) malloc(sizeof(char));
    message[0] = PING;

    sendto(s, (const char *) message, 1, MSG_DONTWAIT, (const struct sockaddr *) sv_addr, sizeof(*sv_addr));
    socklen_t len = sizeof(*sv_addr);
    time_t before = clock();
    message[0] = 123;
    while (clock() - before < TIMEOUT && n <= 0) n = recvfrom(s, (char *) message, 1, MSG_DONTWAIT, (struct sockaddr *) sv_addr, (socklen_t *) &len);
    if (message[0] != ACK) {
        printf("n= %d message =%d", n, (int) message[0]);
        printf("Auth Server is offline\n");
        exit(EXIT_FAILURE);
    }
    printf("Auth Server is online\n");
    free(message);
    return s;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: %s AUTH_SERVER_IP AUTH_SERVER_PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create list of groups and dicts
    grouplist.list = ulist_create(free_glelement);
    pthread_mutex_init(&grouplist.mutex, NULL);

    callbacklist.list = ulist_create(free_cbelement);
    pthread_mutex_init(&callbacklist.mutex, NULL);

    infolist.list = ulist_create(free);
    pthread_mutex_init(&infolist.mutex, NULL);

    // Create sockets
    main_socket = init_AF_UNIX_socket(SERVER_ADDR);
    callback_socket = init_AF_UNIX_socket(CB_SERVER_ADDR);
    auth_socket = init_auth_socket(argv, &sv_addr);

    // Start up new thread to handle connections
    pthread_t main_listener, callback_listener;
    if (pthread_create(&main_listener, NULL, main_listener_thread, NULL) != 0) {
        fprintf(stderr, "Error while creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(main_listener);

    // Start up new thread to handle callbacks
    if (pthread_create(&callback_listener, NULL, callback_listener_thread, NULL) != 0) {
        fprintf(stderr, "Error while creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(callback_listener);

    // Process user commands
    char *line = NULL, groupid[1024] = "", secret[1024] = "", cmd[16] = "";
    size_t size = 0, argN;
    char running = 1;

    while (running) {
        printf(">>> ");
        getline(&line, &size, stdin);
        printf("\33[2K\r");

        argN = sscanf(line, "%" STR(16) "s %" XSTR(1024) "s %" XSTR(1024) "s", cmd, groupid, secret);
        switch (get_option(cmd)) {
        case 0: // exit
            running = 0;
            break;

        case 1: // show
            if (argN != 2) {
                fprintf(stderr, "show <groupid>\n");
                break;
            }
            glelement_t *found = NULL;
            if ((found = (glelement_t *) ulist_find_element_if(grouplist.list, find_glelement, groupid)) != NULL) {
                ssdict_print(found->d);
                printf("\n");
            } else
                printf("Group not found\n");
            break;

        case 2: // create
            if (argN != 2) {
                fprintf(stderr, "create <groupid>\n");
                break;
            }

            // Create group
            switch (create_group(groupid, auth_socket, sv_addr)) {
            case 1:
                printf("Success!\n");
                break;
            case -1:
                printf("Connection error\n");
                break;
            case -2:
                printf("Group already exists\n");
                break;
            }
            break;

        case 3: // delete
            if (argN != 2) {
                fprintf(stderr, "delete <groupid>\n");
                break;
            }
            // delete secret from authserver
            // delete all associated data
            switch (delete_group(groupid, auth_socket, sv_addr)) {
            case 1:
                printf("Success!\n");
                break;
            case 0:
                printf("Group doesn't exist locally\n");
                break;
            case -1:
                printf("Connection to Auth Server failed\n");
                break;
            case -2:
                printf("Auth Server returned an error\n");
                break;
            }
            break;

        case 4:
            // show for each client: PID; connection establishing time and connection close time (if not currently connected)
            ulist_print(infolist.list, print_ilelement);
            printf("\n");
            break;

        default:
            fprintf(stderr, "Usage:\n \texit ---> Disconnects\n\tshow <groupid> ---> Shows Secret and Key-Value pairs from groupid\n\tcreate <groupid>  ---> "
                            "Creates Group with id <groupid> \n\tdelete <groupid> ---> Deletes group with id <groupid>\n\tstatus ---> Shows application "
                            "status\n\n");
            break;
        }
        cmd[0]='\0';
    }

    free(line);
    //? Can't figure out a way to free these without causing an error in the threads
    //? We could, of course, stop the threads first and then do this, but that would
    //? require signaling a recv() to end
    // pthread_mutex_destroy(&grouplist.mutex);
    // pthread_mutex_destroy(&callbacklist.mutex);
    // pthread_mutex_destroy(&infolist.mutex);
    // ulist_free(grouplist.list);
    // ulist_free(callbacklist.list);
    // ulist_free(infolist.list);
}
