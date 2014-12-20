#include <stdio.h>

extern int function(int a);
extern int caller(int a);

int main()
{
    int a = 42;
    a = function(a);
    printf("call function() in main:%d\n", a);

    a = caller(1);
    printf("call caller() in main:%d\n", a);
    return 0;
}

