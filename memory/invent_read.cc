#include <iostream>
#include <thread>
#include <atomic>

using namespace std;
int g_a, g_b;
atomic<int> g_x, g_y;
memory_order lo = memory_order_acquire; //acquire
memory_order so = memory_order_release; //release

void bar1()
{
    g_a = 1;
}

void bar2()
{
    int r1 = g_a;
    int r2 = 0;
    if (r1 == 1)
    {
        r2++;

        if (r1 == 1) r2++;
    }

    if (r2 == 1) cout << "r2 == 1" << endl;
}

int main()
{
    for (int i = 0; i < 1000000; ++i)
    {
        g_a = 0; g_b = 0;

        thread t1(&bar1);
        thread t2(&bar2);

        t1.join();
        t2.join();

    }
}

