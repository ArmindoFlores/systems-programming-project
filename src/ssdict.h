#ifndef _SSDICT_H_
#define _SSDICT_H_

typedef struct ssdict ssdict_t;

ssdict_t *ssdict_create(int capacity);
void ssdict_free(ssdict_t*);

const char *ssdict_get(ssdict_t*, char* key);
int ssdict_set(ssdict_t*, char* key, char* value);

#endif