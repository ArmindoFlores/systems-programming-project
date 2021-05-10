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
    strcpy(vbuffer, value);
    vbuffer[vlen] = '\0';

    unsigned long hash = hash33(key) % d->capacity;

    sspair *found = (sspair*) ulist_find_element_if(d->pairs[hash], find_key, (void*)key);
    if (found != NULL) {
        // Key already exists in the dictionary, so need only change the value
        free(found->value);
        found->value = vbuffer;
    }
    else {
        // Key doesn't exist in the dictionary, so we create it
        sspair *pair = (sspair*) malloc(sizeof(sspair));
        pair->key = (char*) malloc(sizeof(char) * (klen+1));
        pair->value = vbuffer;
        strcpy(pair->key, key);
        pair->key[klen] = '\0';

        if (ulist_pushback(d->pairs[hash], (void*)pair) != 0)
            return 1;
    }

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