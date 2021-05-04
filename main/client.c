#include <stdio.h>
#include "../src/KVS-lib.h"

int main() 
{
   int result = establish_connection("hello", "123");
   
   printf("Result: %d\n", result);

   close_connection();
}
