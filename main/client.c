#include <stdio.h>
#include "../src/KVS-lib.h"

int main() 
{
    int result = establish_connection("group1", "123");

    printf("Result: %d\n", result);

    if (result != 0) return 1;

    result = put_value("chicken4", "egg4");
    
    printf("Result: %d\n", result);

    close_connection();
}
