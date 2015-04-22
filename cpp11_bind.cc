#include <memory>
#include <iostream>
#include <functional>
using namespace std;
using namespace std::placeholders;

void func(int i)
{
    cout << i << endl;
}

template<typename FUNC>
void use_func(FUNC f)
{
    f(1, 2);
}

void test_bind1()
{
    function<void(int)> f = bind(&func, _1);
    // use_func(f);
}

void test_bind2()
{
    use_func(bind(&func, _1));
}

class a
{
    public:
        ~a() { cout << "a destructor" << endl; }

    int rd;
};

class b: public a
{
    public:
        ~b() { cout << "b destructor" << endl; }
        int rd3;
};

int main()
{
    test_bind1();
    test_bind2();

    shared_ptr<b> sp1(new b);
    {
        shared_ptr<a> sp2(sp1);
    }

    cout << "out of scope" << endl;

    return 0;
}

