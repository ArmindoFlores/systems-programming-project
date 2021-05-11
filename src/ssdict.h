#ifndef _SSDICT_H_
#define _SSDICT_H_

typedef struct ssdict ssdict_t;

/*
 * Creates a new (char*, char*) dictionary. Returns NULL if any errors occurr.
 */
ssdict_t *ssdict_create(int capacity);

/*
 * Frees all memory allocated by an ssdict_t
 */
void ssdict_free(ssdict_t*);


/*
 * Retrieve a value from the dictionary. If the key is not in the dictionary,
 * returns NULL.
 */
const char *ssdict_get(ssdict_t*, char* key);

/*
 * If the key is in the dictionary, updates the value.
 * Otherwise, adds an item (key, value) to the dictionary. 
 */
int ssdict_set(ssdict_t*, char* key, char* value);

void ssdict_print(const ssdict_t*);

#endif