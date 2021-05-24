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
	char *secret= (char*) malloc(sizeof(char)*SECRET_SIZE+1);
	time_t t;
	srand((unsigned) time(&t));
	for(char i =0;i<SECRET_SIZE;i++){
		if(rand()%2)
			secret[i]=rand()%10+48; //numbers
		else
			secret[i]=rand()%26+97; //characters
	}
	secret[SECRET_SIZE+1]='\0';
	//printf("%s\n",secret);
	return secret;
}

void *handle_message_thread(void *args){
	handle_message_ta *ta = (handle_message_ta*) args;
	char *gid;
	char *message;
	struct sockaddr_in caddr=ta->client_addr;
	switch(ta->buffer[0]){
		case CREATE_GROUP:
			
			gid= (char*) malloc(sizeof(char)*ta->n-1);

			strncpy(gid,ta->buffer+1,ta->n-1);
			gid[ta->n-1]='\0';
			message=generate_secret();

			//secret[0]=ERROR;// IF error creating;

			printf("Creating Group %s with secret %s\n",gid,message);
			//create dictionary

			sendto(ta->socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,ta->len);

			free(gid);
			free(message);
			
			break;

		case DEL_GROUP:
			//Check if group exists and then delete group else return error.
			gid= (char*) malloc(sizeof(char)*ta->n-1);
			message= (char*) malloc(sizeof(char));

			strncpy(gid,ta->buffer+1,ta->n-1);
			//check if exists
			gid[ta->n-1]='\0';
			printf("Delete group %s\n",gid);
			message[0]=ACK;
			//message[0]=ERROR;
			sendto(ta->socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,ta->len);
			free(gid);
			free(message);
			break;

		case LOGIN:
			//Get secret and gid and return true if matches 0 if not
			message= (char*) malloc(sizeof(char)*SECRET_SIZE+1);
			gid= (char*) malloc(sizeof(char)*ta->n-1);
			strncpy(message,ta->buffer+1,SECRET_SIZE);
			message[16]='\0';
			strncpy(gid,ta->buffer+SECRET_SIZE+1,ta->n-1-SECRET_SIZE);
			printf("Login Attempt Group %s with secret %s\n",gid,message);
			//check
			free(message);
			message= (char*) malloc(sizeof(char));
			message[0]=ACK; //if Right
			//message[0]=ERROR; //if Wrong
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
   
   	int socket;
	socket=init_main_socket(atoi(argv[1]));
	int n, len;
	struct sockaddr_in caddr;
	char *buffer;
	buffer= (char*) malloc(sizeof(char)*(MAX_GROUPID_SIZE+SECRET_SIZE+1+1));
	while(1){
		n=recvfrom(socket, (char *)buffer, 1024,MSG_DONTWAIT, ( struct sockaddr *) &caddr, &len);
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
	return 0;
}