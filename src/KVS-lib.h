#ifndef _KVS_LIB_
#define _KVS_LIB_

typedef enum {
    SUCCESS = 0,
    DISCONNECTED = -1,
    UNKNOWN = -2,
    INVALID = -3,
    ALREADY_CONNECTED = -4,
    SOCK_ERROR = -5
} errors_t;

int establish_connection (char *group_id, char *secret);
int put_value(char *key, char *value);
int get_value(char *key, char **value);
int delete_value(char *key);
int register_callback(char *key, void (*callback_function)(char *));
int close_connection();

#endif
