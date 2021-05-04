#ifndef LIBUTILS_LIST_H
#define LIBUTILS_LIST_H

#include <stdlib.h>

typedef struct ulist ulist_t;

/*
 * Creates a new list. If free_element != NULL, free_element(element) is called
 * for every element in the list when freeing it.
 */
ulist_t *ulist_create(void (*free_element)(void*));

/*
 * Frees all resources allocated by a list, including its elements if 
 * the free_element was specified in ulist_create()
 */
void ulist_free(ulist_t*);


/*
 * Adds a new element to the end of the list
 */
void ulist_pushback(ulist_t*, void* element);

/*
 * Adds a new element to the front of the list
 */
void ulist_pushfront(ulist_t*, void* element);

/*
 * Removes the last element from the list and returns its value
 * Equivalent to ulist_pop(l->size-1)
 */
void *ulist_popback(ulist_t*);

/*
 * Removes the first element from the list and returns its value
 * Equivalend to ulist_pop(0)
 */
void *ulist_popfront(ulist_t*);

/*
 * Returns the ith element of the list or NULL if i >= length
 */
void* ulist_get(const ulist_t*, size_t i);

/*
 * Returns the index of the element
 */
size_t ulist_find(const ulist_t*, void* element);

/*
 * Returns the index of the first element for which verify(element, args) != 0
 */
size_t ulist_find_if(const ulist_t*, int (*verify)(const void*, void*), void* args);

/*
 * Returns the value of the element or NULL if the element isn't on the list
 */
void *ulist_find_element(const ulist_t*, void* element);

/*
 * Returns the value of the first element for which verify(element, args) != 0
 * or NULL if no element satisfy the condition
 */
void *ulist_find_element_if(const ulist_t*, int (*verify)(const void*, void*), void* args);

/*
 * Removes an element from the list
 */
void ulist_remove(ulist_t*, void* element);

/*
 * Removes the first element of the list for which verify(element, args) != 0
 */
void ulist_remove_if(ulist_t*, int (*verify)(const void*, void*), void* args);

/*
 * Removes the ith element from the list and returns its value.
 * This function iterates backwards if i >= length/2
 */
void *ulist_pop(ulist_t*, size_t i);


/*
 * Returns the list's length
 */
size_t ulist_length(const ulist_t*);

/*
 * Prints the list's contents to stdout using print_element(element) to print
 * each individual element
 */
void ulist_print(const ulist_t*, void (*print_element)(const void*));

/*
 * Executes func(element, args) for every element in the list
 */
void ulist_exec(ulist_t*, void (*func)(void*, void*), void* args);

#endif