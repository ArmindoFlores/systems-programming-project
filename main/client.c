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

int main(int argc, char *argv[]) 
{
    int result;
    if (argc >= 3)
        result = establish_connection(argv[1], argv[2]);
    else
        result = establish_connection("group1", "passsword");

    if (result != 1) {
        fprintf(stderr, "An error occurred while connecting (ERRNO %d)\n", result);
        return 1;
    }

    // Process user commands
    char *line = NULL, key[MAX_KEY_SIZE], value[MAX_VALUE_SIZE], cmd[16];
    size_t size = 0;
    while (1) {
        printf(">>> ");
        getline(&line, &size, stdin);

        if (strcmp(line, "exit\n") == 0)
            break;


        if (sscanf(line, "%" STR(16) "s %" XSTR(MAX_KEY_SIZE) "s %" XSTR(MAX_VALUE_SIZE) "s", &cmd, &key, &value) == 3) {
            if (strcmp(cmd, "put") == 0) {
                int r = put_value(key, value);
                if (r != 1)
                    fprintf(stderr, "An error occurred while talking to the server (ERRNO %d)\n", r);
            }
        }

        if (sscanf(line, "%" STR(16) "s %" XSTR(MAX_KEY_SIZE) "s", &cmd, &key) == 2) {
            if (strcmp(cmd, "get") == 0) {
                char *v = NULL;
                int r = get_value(key, &v);
                if (r != 1)
                    fprintf(stderr, "An error occurred while talking to the server (ERRNO %d)\n", r);
                else
                    printf("%s -> %s\n", key, v);
                free(v);
            }
        }
    }

    free(line);
    close_connection();
    printf("Disconnected\n");
}
