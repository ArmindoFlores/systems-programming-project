#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "common.h"
#include "list.h"
#include "ssdict.h"




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

int main() {
   
   	int socket;
	socket=init_main_socket(5000);

	int n, len;
	struct sockaddr_in caddr;
	char buffer[1024];
	while(1){
		n=recvfrom(socket, (char *)buffer, 1024,MSG_DONTWAIT, ( struct sockaddr *) &caddr, &len);
		if(n>0){
			buffer[n]='\0';
			printf("size= %d: %s\n",n, buffer);
			if(strcmp(buffer, "exit") == 0) return 0;
			memset(buffer, 0, sizeof(1024));
		}
	}
	return 0;
}