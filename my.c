#include <stdio.h>
#include "my.h"
#define PI 3.14
#define SQUARE(x) ((x) * (x))

/* area of circle */
int add(int a, int b) { return a + b; }

int main(void)
{
    int n = MAX;                       // from my.h
    printf("%f\n", PI * SQUARE(2));
    printf("%d\n", n);
    printf("%d\n", add(2, 3));
    return 0;
}
