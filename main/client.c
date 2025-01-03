#define  _GNU_SOURCE // Otherwise we get a warning about getline()
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "../src/KVS-lib.h"
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE 65536
#define XSTR(a) STR(a)
#define STR(a) #a

char getOption(char* arg);

void callback1(char *value)
{
    if (value != NULL)
        printf("The value of name was changed to '%s'\n", value);
    else
        printf("The KV-pair name was deleted\n");
}

int main(int argc, char *argv[]) 
{
    int result;
    if (argc == 3)
        if(strlen(argv[2])==16)
            result = establish_connection(argv[1], argv[2]);
        else{
            printf("SECRET has to be a 16 character string\n");
            exit(EXIT_FAILURE);
    }else{
        printf("Usage: %s GROUPID SECRET\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (result != 1) {
        fprintf(stderr, "An error occurred while connecting (ERRNO %d)\n", result);
        return 1;
    }

    if (put_value("name", "francisco") != 1) {
        printf("ERROR\n");
        exit(EXIT_FAILURE);
    }
    if ((result = register_callback("name", callback1)) != 1) {
        printf("Error registering callback (%d)\n", result);
    }

    // Process user commands
    char *line = NULL, key[MAX_KEY_SIZE], value[MAX_VALUE_SIZE], cmd[16];
    size_t size = 0, argN;
    char running=1, r;
    while (running) {
        printf(">>> ");
        getline(&line, &size, stdin);
        argN = sscanf(line, "%" STR(16) "s %" XSTR(MAX_KEY_SIZE) "s %" XSTR(MAX_VALUE_SIZE) "s", &cmd, &key, &value);
        switch(getOption(cmd)){
            case 0: //exit
                running = 0;
                break;

            case 1: //put
                if(argN!=3){
                    fprintf(stderr, "put <key> <value>\n");
                    break;
                }
                r = put_value(key, value);
                if (r != 1)
                    fprintf(stderr, "An error occurred while talking to the server (ERRNO %d)\n", r);
                break;

            case 2: //get
                if(argN!=2){
                    fprintf(stderr, "get <key>\n");
                    break;
                }
                char *v = NULL;
                r = get_value(key, &v);
                if (r != 1)
                    fprintf(stderr, "An error occurred while talking to the server (ERRNO %d)\n", r);
                else
                    printf("%s -> %s\n", key, v);
                free(v);
                break;

            case 3: //delete
                if(argN!=2){
                    fprintf(stderr, "delete <key>\n");
                    break;
                }
                r = delete_value(key);
                if (r != 1)
                    fprintf(stderr, "An error occurred while talking to the server (ERRNO %d)\n", r);
                break;

            default:
                fprintf(stderr, "Usage:\n \texit ---> Disconnects\n\tput <key> <value> ---> Assigns the value to the key\n\tget <key> --> Retrieves value from specified key\n\n");
                break;
        }
    }
    free(line);
    close_connection();
    printf("Disconnected\n");
    return 0;
}


char getOption(char* arg){
    if(strcmp(arg, "exit")==0) return 0;
    if(strcmp(arg, "put")==0)  return 1;
    if(strcmp(arg, "get")==0)  return 2;
    if(strcmp(arg, "delete")==0)  return 3;
    return -1;
}