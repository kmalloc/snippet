#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

using namespace std;

atomic<int> g_x, g_y, g_p, g_g;
memory_order lo = memory_order_relaxed; //acquire
memory_order so = memory_order_relaxed; //release

void bar1()
{
    g_y.store(42, so);
    g_x.store(1, so);
}

void bar2()
{
    int j = 0;
    int i = g_x.load(lo);
    if (i) j = g_y.load(lo);

    g_p = j;
    g_g = i;
}

class cs
{
    shared_ptr<cs> pc;
};

int main()
{
    cs c;
    for (int i = 0; i < 1000000; ++i)
    {
        g_x = 0; g_y = 0; g_p = 0; g_g = 0;

        thread t1(&bar1);
        thread t2(&bar2);

        t1.join();
        t2.join();

        if (g_g == 1 && g_p == 0) cout << "reorder!" << endl;
    }
}

