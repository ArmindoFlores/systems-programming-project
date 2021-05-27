#define  _GNU_SOURCE // Otherwise we get a warning about getline()
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include "KVS-LocalServer.h"
#include "common.h"
#include "list.h"
#include "ssdict.h"

#define XSTR(a) STR(a)
#define STR(a) #a

#define TIMEOUT 5*CLOCKS_PER_SEC

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
    // pthread_mutex_destroy(&element->mutex);
    free(element);
}

int find_glelement(const void *element, void* arg)
{
    return strcmp(((glelement_t*)element)->groupid, (char*) arg) == 0;
}

void print_glelement(void *arg, void *ignore)
{
    glelement_t *element = (glelement_t*) arg;
    printf("%s -> ", element->groupid);
    ssdict_print(element->d);
    printf("\n");
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

    // Add KV pair to the group's dictionary
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
    
    pthread_mutex_unlock(&grouplist.mutex);

    free(key);
    free(value);

    // Notify client that the KV pair has been stored or that an error has occurred
    if (sendall(socket, (char*)&msg, sizeof(msg)) != 0)
        return 0;

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
    if (h->size < sizeof(ksize))
        return 0;

    if (recvall(socket, (char*)&ksize, sizeof(ksize)) != 0)
        return 0;

    if (h->size != ksize + sizeof(ksize))
        return 0;

    key = (char*) calloc(ksize+1, sizeof(char));
    if (key == NULL) {
        // Read all data, but ignore it and exit
        recvall(socket, NULL, ksize);
        msg.type = EINTERNAL;
        sendall(socket, (char*)&msg, sizeof(msg));
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
    glelement_t *group = (glelement_t*) ulist_find_element_if(grouplist.list, find_glelement, groupid);
    if (group == NULL) { 
        // Group was deleted, notify client and close the connection
        free(key);
        msg.type = EGROUP_DELETED;
        sendall(socket, (char*)&msg, sizeof(msg));
        pthread_mutex_unlock(&grouplist.mutex);
        return 0;
    }
    const char *value = ssdict_get(group->d, key);
    pthread_mutex_unlock(&grouplist.mutex);

    msg.type = value == NULL ? KEY_NOTFOUND : KEY_FOUND;
    msg.size = value == NULL ? 0 : strlen(value);

    // Send response
    if (sendall(socket, (char*)&msg, sizeof(msg)) != 0)
        return 0;
    if (msg.type == KEY_FOUND) {
        if (sendall(socket, value, msg.size) != 0)
            return 0;
    }
    return 1;
}

int delete_group(char *groupid,int as,struct sockaddr_in sv_addr)
{
    size_t gidlen=strlen(groupid);
    groupid[gidlen]='\0';
    char *message= (char*) malloc(sizeof(char)*gidlen+1);
    message[0]=DEL_GROUP;
    strncpy(message+1,groupid,gidlen);
    message[1+gidlen]='\0';
    char buffer[1024];  
    int n=-1;

    pthread_mutex_lock(&grouplist.mutex);
    int result = ulist_remove_if(grouplist.list, find_glelement, groupid);
    pthread_mutex_unlock(&grouplist.mutex);

    if(!result){
        sendto(as, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
        free(message);
        socklen_t len=sizeof(sv_addr);
        time_t before = clock();
        while(clock()-before<TIMEOUT && n<=0)
            n=recvfrom(as, (char *)buffer, 1024, MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
        if(buffer[0]==ERROR) return -2;
        if(n<=0) return -1;
    }
    return !result;
}

int create_group(char *groupid, int as, struct sockaddr_in sv_addr)
{
    size_t gidlen = strlen(groupid);
    
    groupid[gidlen] = '\0';
    printf("group : %s size = %d",groupid, gidlen);

    pthread_mutex_lock(&grouplist.mutex);
    glelement_t *group = (glelement_t*) ulist_find_element_if(grouplist.list, find_glelement, groupid);             
    if (group == NULL) { // No information is stored for this group yet
        char *message= (char*) malloc(sizeof(char)*gidlen+1);
        message[0]=CREATE_GROUP;
        strncpy(message+1,groupid,gidlen);
        message[1+gidlen]='\0';
        printf("Sent Group creation request\n");
        char buffer[1024];
        sendto(as, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
        //sendto(as, (const char *)hello, strlen(hello), MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
        int n=-1;
        socklen_t len=sizeof(sv_addr);
        time_t before = clock();
        while(clock()-before<TIMEOUT && n<=0)
            n=recvfrom(as, (char *)buffer, 1024,MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
        if(buffer[0]==ERROR) return -2;
        if(n<=0) return -1;
        printf("The secret for Group %s is %s\n",groupid,buffer+1);
        free(message);



        group = (glelement_t*) malloc(sizeof(glelement_t));
        if (group == NULL) {
            free(groupid);
            fprintf(stderr,"Memory error(internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        group->d = ssdict_create(16);
        group->groupid = (char*) malloc(sizeof(char)*(gidlen+1));
        if (group->d == NULL || group->groupid == NULL) {
            free(group->d);
            free(group->groupid);
            free(groupid);
            fprintf(stderr,"Memory error(internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        strcpy(group->groupid, groupid);
        group->groupid[gidlen] = '\0';

        if (ulist_pushback(grouplist.list, group) != 0) {
            free(groupid);
            free_glelement(group);
            fprintf(stderr,"Memory error(internal error)\n");
            pthread_mutex_unlock(&grouplist.mutex);
            return -1;
        }

        


    }
    else{
        pthread_mutex_unlock(&grouplist.mutex);
        return -2;
    }
    pthread_mutex_unlock(&grouplist.mutex);
    return 1;
}

int msg_delete_value(int socket, msgheader_t *h, char *groupid)
{
    return 0;
}

int msg_register_callback(int socket, msgheader_t *h, char *groupid) 
{
    return 0;
}

int login_auth(char *gid, char *secret, int as,struct sockaddr_in sv_addr)
{
    char *message = (char*) malloc(strlen(gid)+16+1);
    message[0]=LOGIN;
    strncpy(message+1,secret,16);
    strncpy(message+1+16, gid, strlen(gid));
    printf("Sending login attempt %s\n",message);
    sendto(as, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &sv_addr, sizeof(sv_addr));
    char buffer[1024];
    int n = -1;
    socklen_t len = sizeof(sv_addr);
    time_t before = clock();
    while(clock()-before<TIMEOUT && n<0)
        n=recvfrom(as, (char *)buffer, 1024,MSG_DONTWAIT, (struct sockaddr *) &sv_addr, &len);
    if(buffer[0]==ERROR) return -2;
    if(n<0) return -1;
    
    free(message);
       
    return 1;
}

void *connection_handler_thread(void *args)
{
    int running = 1;
    conn_handler_ta *ta = (conn_handler_ta*) args;
    char *groupid = NULL, secret[SECRET_SIZE] = "";
    size_t gidlen;

    if (recvall(ta->socket, (char*)&gidlen, sizeof(gidlen)) != 0 ||
        gidlen > MAX_GROUPID_SIZE) {
        close(ta->socket);
        free(ta);
        printf("Disconnected\n");
        return NULL;
    }

    groupid = (char*) calloc((gidlen + 1), sizeof(char));
    if (groupid == NULL) {
        close(ta->socket);
        free(ta);
        printf("Disconnected (internal error)\n");
        return NULL;
    }

    if (recvall(ta->socket, groupid, gidlen) != 0 ||
        recvall(ta->socket, secret, SECRET_SIZE)) {
        running = 0;
    }

    if (running && ulist_find_element_if(grouplist.list, find_glelement, groupid) == NULL) { //check if group exists
        running = 0;
        //dont accept
    }else if(login_auth(groupid,secret,ta->auth_socket,ta->sv_addr)!=1){
        running= 0;
    }

    printf("Sending response...\n");
    
    if (sendall(ta->socket, (char*)&running, sizeof(running)) != 0)
        running = 0;

    msgheader_t header;
    while (running) {
        if (recvall(ta->socket, (char*)&header, sizeof(header)) != 0) 
            break;
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
                running = msg_register_callback(ta->socket, &header, groupid);
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
        args->auth_socket = ta->auth_socket;
        args->sv_addr = ta->sv_addr;
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

char get_option(char* arg)
{
    if (strcmp(arg, "exit") == 0) return 0;
    if (strcmp(arg, "show") == 0)  return 1;
    if (strcmp(arg, "create") == 0)  return 2;
    if (strcmp(arg, "delete") == 0)  return 3;
    if (strcmp(arg, "status") == 0)  return 4;
    return -1;
}

int init_auth_socket(char **argv, struct sockaddr_in* sv_addr){

    int s,n=-1; 
    if ((s= socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error while creating the socket\n");
        exit(EXIT_FAILURE);
    }
    sv_addr->sin_family = AF_INET;
    printf("AuthServer Address=%s Port=%s\n",argv[1],argv[2]);
    sv_addr->sin_port =htons(atoi(argv[2]));
    inet_aton(argv[1], &sv_addr->sin_addr);

    char *message = (char*) malloc(sizeof(char));
    message[0] = PING;

    sendto(s, (const char *)message, 1, MSG_DONTWAIT, (const struct sockaddr *) sv_addr, sizeof(*sv_addr));
    socklen_t len=sizeof(*sv_addr);
    time_t before = clock();
    message[0] = 123;
    while(clock()-before<TIMEOUT && n<=0)
        n=recvfrom(s, (char *)message, 1,MSG_DONTWAIT, ( struct sockaddr *) sv_addr, (socklen_t*)&len);
    if(message[0]!=ACK){
        printf("n= %d message =%d",n,(int)message[0]);
        printf("AuthServer is offline\n");
        exit(EXIT_FAILURE);
    } 
    printf("AuthServer is online\n");
    free(message);
    //char *hello ="hello";
    //sendto(s, (const char *)hello, strlen(hello), MSG_DONTWAIT, (const struct sockaddr *) sv_addr, sizeof(*sv_addr));

    return s;
}


int main(int argc, char *argv[]) 
{
    if(argc<3){
        printf("Input authserver IP address and port number\n");
        exit(-1);
    }
    // Create list of groups and dicts
    grouplist.list = ulist_create(free_glelement);
    pthread_mutex_init(&grouplist.mutex, NULL);

    // Create local socket
    int s = init_main_socket(SERVER_ADDR);
    int as;
    struct sockaddr_in sv_addr;
    as = init_auth_socket(argv,&sv_addr);
    // Start up new thread to handle connections
   
    //main_listener_ta args = { s }; 
    main_listener_ta *args = (main_listener_ta*) malloc(sizeof(main_listener_ta));
    if (args == NULL) {
            printf("Error allocating memory!\n");
    }

    args->socket = s;

    args->auth_socket = as;
    args->sv_addr = sv_addr;


    pthread_t main_listener;
    if (pthread_create(&main_listener, NULL, main_listener_thread, args) != 0) {
        fprintf(stderr, "Error while creating thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(main_listener);

    // Process user commands
    char *line = NULL, groupid[1024] = "", secret[1024] = "", cmd[16] = "";
    size_t size = 0, argN;
    char running = 1;

    while (running) {
        printf(">>> ");
        getline(&line, &size, stdin);
        argN = sscanf(line, "%" STR(16) "s %" XSTR(1024) "s %" XSTR(1024) "s", cmd, groupid, secret);
        switch(get_option(cmd)){
            case 0: //exit
                running = 0;
                break;

            case 1: //show
                if (argN != 2) {
                    fprintf(stderr, "show <groupid>\n");
                    break;
                }
                glelement_t *found = NULL;
                if ((found = (glelement_t*) ulist_find_element_if(grouplist.list, find_glelement, groupid)) != NULL) {
                    ssdict_print(found->d);
                    printf("\n");
                }
                else
                    printf("Group not found\n");
                break;

            case 2: //create
                if (argN != 2) {
                    fprintf(stderr, "create <groupid>\n");
                    break;
                }

                //Create group 
                switch (create_group(groupid, as, sv_addr)) {
                    case 1:
                        printf("Success!\n");
                        break;
                    case -1:
                        printf("Connection error\n");
                        break;
                    case -2:
                        printf("Group id already exists\n");
                        break;

                }
                break;

            case 3: //delete
                if (argN != 2){
                    fprintf(stderr, "delete <groupid>\n");
                    break;
                }
                //delete secret from authserver
                //delete all associated data
                switch (delete_group(groupid,as, sv_addr)) {
                    case 1:
                        printf("Success!\n");
                        break;
                    case 0:
                        printf("Group id doesn't exists\n");
                        break;
                    case -1:
                        printf("Connection to auth Server failed\n");
                        break;
                    case -2:
                        printf("Auth server returned error\n");
                        break;
                }
                break;

            case 4: 
                //show for each client: PID; connection establishing time and connection close time (if not currently connected)

                break;

            default:
                fprintf(stderr, "Usage:\n \texit ---> Disconnects\n\tshow <groupid> ---> Shows Secret and Key-Value pairs from groupid\n\tcreate <groupid>  --> Creates Group with id <groupid> \n\tdelete <groupid> ---> Deletes group with id <groupid>\n\tstatus ---> Shows application status\n\n");
                break;
        }
    }

    free(line);
    pthread_mutex_destroy(&grouplist.mutex);
    ulist_free(grouplist.list);
}
