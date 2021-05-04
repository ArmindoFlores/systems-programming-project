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
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE 65536

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

int msg_put_value(int socket, msgheader_t *h) 
{
   msgheader_t msg;
   msg.size = 0;
   char *key, *value;
   int ksize, vsize;
   
   // Make sure the message size makes sense (has to contain at least ksize and vsize)
   if (h->size < sizeof(ksize) + sizeof(vsize))
      return 0;
   
   if (recvall(socket, (char*)&ksize, sizeof(ksize)) != 0)
      return 0;

   if (recvall(socket, (char*)&vsize, sizeof(vsize)) != 0)
      return 0;
   
   // Make sure the client is sending valid sizes
   if (ksize == 0 || vsize == 0 || ksize >= MAX_KEY_SIZE || vsize >= MAX_VALUE_SIZE)
      return 0;

   // Allocate memory for the KV pair
   key = (char*) calloc(ksize+1, sizeof(char));
   value = (char*) calloc(vsize+1, sizeof(char));

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
   printf("[%d] Stored the KV pair (%s, %s)\n", socket, key, value);

   msg.type = ACK;

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
   conn_handler_ta *ta = (conn_handler_ta*) args;

   msgheader_t header;
   int running = 1;
   while (running) {
      if (recvall(ta->socket, (char*)&header, sizeof(header)) != 0) 
         break;
      switch (header.type) {
         case PUT_VALUE:
            running = msg_put_value(ta->socket, &header);
            break;
         case GET_VALUE:
            running = msg_get_value();
            break;
         case REGISTER_CALLBACK:
            running = msg_register_callback();
            break;
         default:
            running = 0;
            break;
      }
   }

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

}
