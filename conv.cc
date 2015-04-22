#include <stdio.h>
#include <string>

extern int g_func(int a);
extern int g_share;

class conv
{
    public:
        conv() {}
        virtual ~conv() {}
        explicit conv(const std::string& f): m_f(f) {}
        explicit conv(const conv& c) { m_f = c.m_f; }
    private:
        std::string m_f;
};

void convf(const conv& v)
{
}

// valid in c++11
// otherwise only valid for POD
conv v = {};

int main()
{
    std::string s("s");
    convf(conv(s));

    int a = 42;
    /* a = g_func(a); */

    a += 1;

    printf("dummy print 1\n");
    printf("dummy print 2\n");

    /* printf("external int:%d\n", g_share); */

    /* g_share += a; */

    /* g_func2(); */

    return 0;
}

