#include "list.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct ulistelement {
    struct ulistelement *prev, *next;
    void* value;
} ulistelement_t;

struct ulist {
    ulistelement_t *head, *tail;
    size_t size;
    void (*free_element)(void*);
};

int verify_equal(const void* item, void* value) { return item == value; }

void ulist_remove_element(ulist_t *l, ulistelement_t *element)
{
    if (l->head == element)
        l->head = element->next;
    else
        element->prev->next = element->next;
    if (l->tail == element)
        l->tail = element->prev;
    else
        element->next->prev = element->prev;
}

ulist_t *ulist_create(void (*free_element)(void*))
{
    ulist_t *new_list = (ulist_t*) malloc(sizeof(ulist_t));
    if (new_list == NULL) 
        return NULL;
    new_list->free_element = free_element;
    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->size = 0;

    return new_list;
}

void ulist_free(ulist_t* l)
{
    ulistelement_t *aux, *temp;
    for (aux = l->head, temp = NULL; aux != NULL; aux = aux->next) {
        free(temp);
        if (l->free_element != NULL)
            l->free_element(aux->value);
        temp = aux;
    }
    free(temp);
    free(l);
}

int ulist_pushback(ulist_t* l, void* element)
{
    ulistelement_t *new_element = (ulistelement_t*) malloc(sizeof(ulistelement_t));
    if (new_element == NULL) 
        return 1;
    new_element->value = element;

    new_element->next = NULL;
    new_element->prev = l->tail;
    if (l->size == 0) {
        l->head = new_element;
        l->tail = l->head;
    }
    else {
        l->tail->next = new_element;
        new_element->prev = l->tail;
        l->tail = new_element;
    }
    l->size++;
    return 0;
}

int ulist_pushfront(ulist_t* l, void* element)
{
    ulistelement_t *new_element = (ulistelement_t*) malloc(sizeof(ulistelement_t));
    if (new_element == NULL) 
        return 1;
    new_element->value = element;

    new_element->next = l->head;
    new_element->prev = NULL;
    if (l->size == 0) {
        l->head = new_element;
        l->tail = l->head;
    }
    else {
        l->head->prev = new_element;
        new_element->next = l->head;
        l->head = new_element;
    }
    l->size++;
    return 0;
}

void *ulist_popback(ulist_t* l)
{
    void *result = NULL;
    if (l->size > 0) {
        ulistelement_t *aux = l->tail;
        result = aux->value;
        ulist_remove_element(l, aux);
        l->size--;
        free(aux);
    }
    return result;
}

void *ulist_popfront(ulist_t* l)
{
    void *result = NULL;
    if (l->size > 0) {
        ulistelement_t *aux = l->head;
        result = aux->value;
        ulist_remove_element(l, aux);
        l->size--;
        free(aux);
    }
    return result;
}

void *ulist_get(const ulist_t* l, size_t i)
{
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next, i--) {
        if (i == 0) 
            return aux->value;
    }
    return NULL;
}

size_t ulist_find(const ulist_t* l, void* element)
{
    return ulist_find_if(l, verify_equal, element);
}

size_t ulist_find_if(const ulist_t* l, int (*verify)(const void*, void*), void* args)
{
    size_t i = 0;
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next, i++) {
        if (verify(aux->value, args)) {
            return i;
        }
    }
    return -1;
}

void *ulist_find_element(const ulist_t* l, void* element)
{
    return ulist_find_element_if(l, verify_equal, element);
}

void *ulist_find_element_if(const ulist_t* l, int (*verify)(const void*, void*), void* args)
{
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next) {
        if (verify(aux->value, args))
            return aux->value;
    }
    return NULL;
}

void ulist_remove(ulist_t* l, void* element)
{
    ulist_remove_if(l, verify_equal, element);
}

void ulist_remove_if(ulist_t* l, int (*verify)(const void*, void*), void* args)
{
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next) {
        if (verify(aux->value, args)) {
            ulist_remove_element(l, aux);
            
            if (l->free_element != NULL)
                l->free_element(aux->value);
            free(aux);
            l->size--;
            break;
        }
    }
}

void *ulist_pop(ulist_t* l, size_t i)
{
    void *result = NULL;
    if (i < l->size / 2) {
        for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next, i--) {
            if (i == 0) {
                result = aux->value;
                ulist_remove_element(l, aux);
                free(aux);
                l->size--;
                break;
            }
        }
    }
    else {
        for (ulistelement_t *aux = l->tail; aux != NULL; aux = aux->prev, i--) {
            if (i == 0) {
                result = aux->value;
                ulist_remove_element(l, aux);
                free(aux);
                l->size--;
                break;
            }
        }
    }
    return result;
}

size_t ulist_length(const ulist_t* l)
{
    return l->size;
}

void ulist_print(const ulist_t* l, void (*print_element)(const void*))
{
    printf("[");
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next) {
        print_element(aux->value);
        if (aux->next != NULL)
            printf(", ");
    }
    printf("]");
}

void ulist_exec(ulist_t* l, void (*func)(void*, void*), void* args)
{
    for (ulistelement_t *aux = l->head; aux != NULL; aux = aux->next)
        func(aux->value, args);
}