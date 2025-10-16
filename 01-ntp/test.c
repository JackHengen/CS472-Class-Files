#include <stdio.h>
#include <stdint.h>
#include <limits.h>

void main(){
int32_t x = INT_MAX;
x = x + 1;   // undefined behavior
             //
printf("%d",x);
}
