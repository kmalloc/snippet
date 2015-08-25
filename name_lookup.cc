#include <iostream>
using namespace std;

template <typename T>
int t(T i)
{
    // unqualified name lookup
    f(i);
}

int f()
{
    return 42;
}

int f(int i)
{
    return i + 42;
}

int main()
{
    return t(1);
}

