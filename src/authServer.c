#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netinet/in.h>
#include "common.h"
#include "list.h"
#include "ssdict.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "authServer.h"


int init_main_socket(int port){
	int s;
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port =htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "Error while creating the socket\n");
		exit(EXIT_FAILURE);
	}

	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Error while binding the address\n");
		exit(EXIT_FAILURE);
	}

	return s;
}


char* generate_secret(){
	char *secret = (char*) malloc(sizeof(char)*SECRET_SIZE+2);
	time_t t;
	srand((unsigned) time(&t));
	for(char i =1;i<SECRET_SIZE+1;i++){
		if(rand()%2)
			secret[i]=rand()%10+48; //numbers
		else
			secret[i]=rand()%26+97; //characters
	}
	secret[0]=ACK;
	secret[SECRET_SIZE+1]='\0';
	return secret;
}

void *handle_message_thread(void *args){
	handle_message_ta *ta = (handle_message_ta*) args;
	char *gid;
	char *message;
	struct sockaddr_in caddr=ta->client_addr;
	switch(ta->buffer[0]){
		case CREATE_GROUP:
			
			gid = (char*) malloc(sizeof(char)*(ta->n-1));
			memcpy(gid, ta->buffer+1, ta->n-1);
			gid[ta->n-2] = '\0';
			message = generate_secret();

			pthread_mutex_lock(&ta->dict_mutex);
			if (ssdict_get(ta->d, gid) == NULL) {
				if (ssdict_set(ta->d, gid, message) != 0) {
					pthread_mutex_unlock(&ta->dict_mutex);
					message[0] = ERROR;// IF error creating;
					printf("Error creating group (memory error)\n");
				}
				else {
					pthread_mutex_unlock(&ta->dict_mutex);
					printf("Creating group %s with secret %s\n", gid, message+1);
				}
			}
			else {
				pthread_mutex_unlock(&ta->dict_mutex);
				message[0] = ERROR;// IF error creating;
				printf("Error creating group (group already exists)\n");
			}
			
			sendto(ta->socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,ta->len);

			free(gid);
			free(message);
			break;

		case DEL_GROUP:
			//Check if group exists and then delete group else return error.
			gid = (char*) malloc(sizeof(char)*ta->n-1);
			message = (char*) malloc(sizeof(char));

			strncpy(gid, ta->buffer+1, ta->n-1);
			gid[ta->n-1] = '\0';

			//check if exists
			pthread_mutex_lock(&ta->dict_mutex);
			if (ssdict_get(ta->d, gid) == NULL) {
				ssdict_set(ta->d, gid, NULL);
				pthread_mutex_unlock(&ta->dict_mutex);
				printf("Delete group %s\n", gid);
				message[0] = ACK;
			}
			else {
				pthread_mutex_unlock(&ta->dict_mutex);
				message[0] = ERROR;
			}
			sendto(ta->socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,ta->len);
			free(gid);
			free(message);
			break;

		case LOGIN:
			//Get secret and gid and return true if matches 0 if not
			message = (char*) malloc(sizeof(char)*(SECRET_SIZE+1));
			gid = (char*) malloc(sizeof(char)*ta->n-1);
			strncpy(message, ta->buffer+1, SECRET_SIZE);
			message[SECRET_SIZE] = '\0';
			strncpy(gid,ta->buffer+SECRET_SIZE+1,ta->n-1-SECRET_SIZE);
			gid[ta->n-SECRET_SIZE-1]='\0';
			printf("Login Attempt Group %s with secret %s\n", gid, message);

			free(message);
			message = (char*) malloc(sizeof(char));

			const char *value = ssdict_get(ta->d, gid);
			if (value == NULL || strcmp(value, gid) != 0) 
				message[0] = ERROR;
			else message[0] = ACK;

			sendto(ta->socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,ta->len);
			free(gid);
			free(message);
			break;

	}
	free(ta->buffer);
	return NULL;
}


int main(int argc, char *argv[]){

	if(argc <2){
		printf("Input port number\n");
        exit(-1);
	}

	ssdict_t *groups = ssdict_create(16);
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
   
   	int socket;
	socket=init_main_socket(atoi(argv[1]));
	int n, len = 0;
	struct sockaddr_in caddr;
	char *buffer;
	buffer= (char*) malloc(sizeof(char)*(MAX_GROUPID_SIZE+SECRET_SIZE+1+1));
	while(1){
		n=recvfrom(socket, (char *)buffer, MAX_GROUPID_SIZE+SECRET_SIZE+1+1,MSG_DONTWAIT, ( struct sockaddr *) &caddr, (socklen_t*)&len);
		if(n>0){
			buffer[n]='\0';
			//printf("client: %s size= %d: %s\n",inet_ntoa(caddr.sin_addr),n, buffer);
			handle_message_ta *args = (handle_message_ta*) malloc(sizeof(handle_message_ta));
        	if (args == NULL) {
            	printf("Error allocating memory!\n");
        	}
        	args->socket = socket;
	        args->client_addr = caddr;
	        args->buffer = buffer;
	        args->n=n+1;
	        args->len=len;
			args->d = groups;
			args->dict_mutex = mutex;

	        pthread_t child;
	        if (pthread_create(&child, NULL, handle_message_thread, args) != 0) {
	            fprintf(stderr, "Error while creating thread\n");
	            free(args);
	            continue;
	        }
	        if (pthread_detach(child) != 0) {
	            fprintf(stderr, "Error while detaching thread\n");
	            continue;
	        }
	        buffer= (char*) malloc(sizeof(char)*(MAX_GROUPID_SIZE+SECRET_SIZE+1+1));
	   	}
	}
	ssdict_free(groups);
	pthread_mutex_destroy(&mutex);
	return 0;
}