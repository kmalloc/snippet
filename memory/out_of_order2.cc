#include <iostream>
#include <thread>
#include <atomic>

using namespace std;

volatile int g_x = 0, g_y = 0;
volatile int g_a = 0, g_b = 0;

void foo1()
{
    g_x = 42;
    g_a = g_y;
}

void foo2()
{
    g_y = 24;
    g_b = g_x;
}

atomic<int> g_a2, g_b2;
atomic<int> g_x2, g_y2;
memory_order order = memory_order_relaxed;

void bar1()
{
    register int t = 0;
    g_x2.store(42, order);
    t = g_y2.load(order);
    g_a2 = t;
}

void bar2()
{
    register int t = 0;
    g_y2.store(24, order);
    t = g_x2.load(order);
    g_b2 = t;
}

atomic<int> g_x3, g_y3 = {3}, g_l = {0};
atomic<int> g_reorder;

void foo3()
{
    register int t = 0;
    g_x3.store(1, order);
    t = g_y3.load(order);
    g_l = t;
}

void foo4()
{
    while (g_x3.load(order) != 1);
    if (g_y3.load(order) != 2) g_reorder++;
}


int main()
{
    /*
    for (int i = 0; i < 10000; ++i)
    {
        g_x = g_y = g_a = g_b = 0;
        thread t1(&foo1);
        thread t2(&foo2);

        t1.join();
        t2.join();
        if (g_a == 0 && g_b == 0) cout << "g_a == g_b == 0" << endl;
    }
    */

    for (int i = 0; i < 1000000; ++i)
    {
        g_a2 = 0;
        g_b2 = 0;
        g_x2.store(0, memory_order_seq_cst);
        g_y2.store(0, memory_order_seq_cst);
        thread t1(&bar1);
        thread t2(&bar2);

        t1.join();
        t2.join();
        if (g_a2.load(order) == 0 && g_b2.load(order) == 0)
        {
            cout << "g_a2 == g_b2 == 0" << endl;
        }
    }

    /*
    for (int i = 0; i < 100000; ++i)
    {
        g_x3 = 0; g_y3 = 0;

        thread t1(&foo3);
        thread t2(&foo4);

        t1.join();
        t2.join();
    }
    cout << "reorder found:" << g_reorder << endl;
    */

    return 0;
}

