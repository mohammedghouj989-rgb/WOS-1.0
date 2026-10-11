#include <stdio.h>
#include "mylib.h"

int main() {
    int result1 = add(5, 3);
    int result2 = sub(10, 4);
    printf("add(5, 3) = %d\n", result1);
    printf("sub(10, 4) = %d\n", result2);
    return 0;
}
