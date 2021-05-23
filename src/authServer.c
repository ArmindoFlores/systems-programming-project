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


int main(int argc, char *argv[]) {

	if(argc <2){
		printf("Input port number\n");
        exit(-1);
	}
   
   	int socket;
	socket=init_main_socket(atoi(argv[1]));
	int n, len;
	struct sockaddr_in caddr;
	char buffer[1024];
	char *gid;
	char *message;
	while(1){
		memset(&caddr, 0, sizeof(caddr));

		n=recvfrom(socket, (char *)buffer, 1024,MSG_DONTWAIT, ( struct sockaddr *) &caddr, &len);
		if(n>0){
			buffer[n]='\0';
			printf("client: %s size= %d: %s\n",inet_ntoa(caddr.sin_addr),n, buffer);
			switch(buffer[0]){

				case CREATE_GROUP:
					
					gid= (char*) malloc(sizeof(char)*n-1);
					
					strncpy(gid,buffer+1,n-1);
					message=generate_secret();
					//secret[0]=ERROR;// IF error creating;
					printf("Creating Group %s with secret %s\n",gid,message);
					//create dictionary

					sendto(socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,len);



					free(gid);
					free(message);
					
					break;

				case DEL_GROUP:
					//Check if group exists and then delete group else return error.
					gid= (char*) malloc(sizeof(char)*n-1);
					message= (char*) malloc(sizeof(char));
					strncpy(gid,buffer+1,n-1);
					//check if exists
					printf("Delete group %s",gid);
					message[0]=ACK;
					//message[0]=ERROR;
					sendto(socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,len);
					free(gid);
					free(message);
					break;

				case LOGIN:
					//Get secret and gid and return true if matches 0 if not
					message= (char*) malloc(sizeof(char)*SECRET_SIZE+1);
					gid= (char*) malloc(sizeof(char)*n-1);
					strncpy(message,buffer+1,SECRET_SIZE);
					message[16]='\0';
					strncpy(gid,buffer+SECRET_SIZE+1,n-1-SECRET_SIZE);
					printf("Login Attempt Group %s with secret %s\n",gid,message);
					sendto(socket, (const char *)message, strlen(message), MSG_DONTWAIT, (const struct sockaddr *) &caddr,len);
					free(gid);
					free(message);
					break;

				default:
					//return invalid
					break;





			}





			memset(buffer, 0, sizeof(1024));
		}
	}
	return 0;
}