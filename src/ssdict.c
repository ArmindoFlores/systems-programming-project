#include "ssdict.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key, *value;
} sspair;

struct ssdict {
    ulist_t **pairs;
    int capacity, size;
};

unsigned long hash33(char* str) {
    unsigned long acc = 0;
    for (char* aux = str; *aux != '\0'; aux++)
        acc = acc * 33 + *aux;
    return acc;
}

void free_sspair(void *arg)
{
    sspair* pair = (sspair*) arg;
    free(pair->key);
    free(pair->value);
    free(pair);
}

int find_key(const void* element, void* args)
{
    char *key = ((sspair*) element)->key, *t_key = (char*) args;
    return strcmp(key, t_key) == 0;
}

void add_to_dict(void *element, void* args)
{
    ssdict_t *d = (ssdict_t*) args;
    sspair *pair = (sspair*) element;

    ssdict_set(d, pair->key, pair->value);
}

int ssdict_resize(ssdict_t *d, int capacity)
{
    printf("Resizing from %d to %d...", d->capacity, capacity);
    if (capacity == d->capacity || capacity <= 0)
        return 1;

    // Create a new dict with the necessary capacity
    ssdict_t *new_dict = ssdict_create(capacity);
    if (new_dict == NULL)
        return 1;

    // Copy each element from 'd' into 'new_dict'
    for (int i = 0; i < d->capacity; i++)
        ulist_exec(d->pairs[i], add_to_dict, new_dict);
    
    // Make sure the copy was successfull
    if (new_dict->size == d->size) {
        // Free previous dictionary's array of lists
        for (int i = 0; i < d->capacity; i++)
            ulist_free(d->pairs[i]);
        free(d->pairs);

        // Now swap
        d->pairs = new_dict->pairs;
        d->capacity = new_dict->capacity;
        d->size = new_dict->size;

        // We don't need new_dict anymore, only its contents
        free(new_dict);
        printf("Success!\n");

        return 0;
    }
    else {
        // Copy wasn't successfull - free new_dict and notify user
        ssdict_free(new_dict);
        return 1;
    }
}

ssdict_t *ssdict_create(int capacity)
{
    if (capacity <= 0)
        return NULL;

    ssdict_t *new_dict = (ssdict_t*) malloc(sizeof(ssdict_t));
    if (new_dict == NULL)
        return NULL;

    new_dict->capacity = capacity;
    new_dict->size = 0;

    // Allocate an array of lists (where kv pairs will be stored)
    new_dict->pairs = (ulist_t**) malloc(sizeof(ulist_t*) * new_dict->capacity);
    if (new_dict->pairs == NULL) {
        free(new_dict);
        return NULL;
    }

    // Allocate memory for each individual list in the array
    for (int i = 0; i < new_dict->capacity; i++) {
        // If list creation fails, free all previous lists and return NULL
        if ((new_dict->pairs[i] = ulist_create(free_sspair)) == NULL) {
            for (int j = i-1; j >= 0; j--)
                ulist_free(new_dict->pairs[j]);
            free(new_dict);
            return NULL;
        }
    }

    return new_dict; 
}

void ssdict_free(ssdict_t *d)
{
    for (int i = 0; i < d->capacity; i++)
        ulist_free(d->pairs[i]);
    free(d->pairs);
    free(d);
}

int ssdict_set(ssdict_t *d, char *key, char *value)
{
    size_t klen = strlen(key), vlen = strlen(value);
    char *vbuffer = (char*) malloc(sizeof(char) * (vlen+1));
    if (vbuffer == NULL)
        return 1;

    strcpy(vbuffer, value);
    vbuffer[vlen] = '\0';

    unsigned long hash = hash33(key) % d->capacity;

    // Now we search for the key in the hash's corresponding list
    sspair *found = (sspair*) ulist_find_element_if(d->pairs[hash], find_key, (void*)key);
    if (found != NULL) {
        // Key already exists in the dictionary, so we need only change the value
        free(found->value);
        found->value = vbuffer;
    }
    else {
        // Key doesn't exist in the dictionary, so we create it
        sspair *pair = (sspair*) malloc(sizeof(sspair));
        if (pair == NULL) {
            free(vbuffer);
            return 1;
        }
        pair->key = (char*) malloc(sizeof(char) * (klen+1));
        if (pair->key == NULL) {
            free(vbuffer);
            free(pair);
            return 1;
        }

        pair->value = vbuffer;
        strcpy(pair->key, key);
        pair->key[klen] = '\0';

        if (ulist_pushback(d->pairs[hash], (void*)pair) != 0) {
            free(vbuffer);
            free(pair->key);
            free(pair);
            return 1;
        }

        d->size++;
    }

    // Check if dictionary needs resizing
    // TODO: This is probably not what we want. A better way of doing this
    // TODO: would be to check if this list's length is greater than a certain
    // TODO: threshold. This way we could resize if, for example, few items were in
    // TODO: the dictionary but a long chain had been created (slowing lookup times).
    // TODO: Maybe research what the better condition is?
    if (d->size >= 2*d->capacity)
        ssdict_resize(d, d->capacity*4);

    return 0;
}

const char* ssdict_get(ssdict_t *d, char *key)
{
    unsigned long hash = hash33(key) % d->capacity;
    sspair *pair = (sspair*) ulist_find_element_if(d->pairs[hash], find_key, (void*)key);
    if (pair == NULL)
        return NULL;
    return pair->value;
}

void print_elements(void* arg, void* ignore)
{
    sspair *pair = (sspair*) arg;
    printf("    \"%s\": \"%s\",\n", pair->key, pair->value);
}

void ssdict_print(const ssdict_t *d)
{
    printf("{\n");
    for (int i = 0; i < d->capacity; i++)
        ulist_exec(d->pairs[i], print_elements, NULL);
    printf("}");
}