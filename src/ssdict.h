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
 * Returns 0 upon sucess, 1 otherwise.
 */
int ssdict_set(ssdict_t*, char* key, char* value);

/*
 * Resizes an existing dictionary. This is done by creating a new one and 
 * copying & rehashing all elements. Returns 0 if successfull, 1 otherwise.
 */
int ssdict_resize(ssdict_t*, int capacity);

/*
 * Prints every element inside a dictionary in the following format:
 * {
 *      "first_key": "first_value",
 *      "second_key": "second_value",
 * }
 */
void ssdict_print(const ssdict_t*);

#endif