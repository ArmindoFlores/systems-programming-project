#include <stdio.h>
#include "../src/KVS-lib.h"

int main() 
{
    int result = establish_connection("hello", "123");

    printf("Result: %d\n", result);

    result = establish_connection("hello", "123"); // Should return an error code

    printf("Result: %d\n", result);

    close_connection();
}
