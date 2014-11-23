#include <iostream>
#include <thread>
#include <atomic>

using namespace std;
atomic<int> g_a, g_b;
atomic<int> g_x, g_y;
memory_order order = memory_order_relaxed;

void bar1()
{
    register int t = 0;
    g_x.store(42, order);
    t = g_y.load(order);
    g_a = t;
}

void bar2()
{
    register int t = 0;
    g_y.store(24, order);
    t = g_x.load(order);
    g_b = t;
}

int main()
{
    for (int i = 0; i < 1000000; ++i)
    {
        g_a = 0; g_b = 0;
        g_x = 0; g_y =0;
        thread t1(&bar1);
        thread t2(&bar2);

        t1.join();
        t2.join();
        if (g_a.load(order) == 0 && g_b.load(order) == 0)
        {
            cout << "g_a == g_b == 0" << endl;
        }
    }
}

