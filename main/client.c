#include <stdio.h>
#include "../src/KVS-lib.h"

int main() 
{
    int result = establish_connection("hello", "123");

    printf("Result: %d\n", result);

    if (result != 0) return 1;

    result = put_value("this is my key", "this is my value");
    
    printf("Result: %d\n", result);

    close_connection();
}
