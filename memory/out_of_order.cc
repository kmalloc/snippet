#include <iostream>
#include <pthread.h>

using namespace std;

volatile int g_x __attribute__((align(8))) = 0, g_y __attribute__((align(8))) = 0;
volatile int g_a __attribute__((align(8))) = 0, g_b __attribute__((align(8))) = 0;

void* foo1(void*)
{
    g_x = 42;
    g_a = g_y;
    return NULL;
}

void* foo2(void*)
{
    g_y = 24;
    g_b = g_x;
    return NULL;
}

int main()
{
    for (int i = 0; i < 100000; ++i)
    {
        pthread_t t1, t2;

        g_x = g_y = g_a = g_b = 0;
        pthread_create(&t1, NULL, &foo1, NULL);
        pthread_create(&t2, NULL, &foo2, NULL);

        pthread_join(t1, NULL);
        pthread_join(t2, NULL);

        if (g_a == 0 && g_b == 0) cout << "g_a == g_b == 0" << endl;
    }

    return 0;
}

