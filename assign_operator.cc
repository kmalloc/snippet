#include <iostream>
using namespace std;

class Base
{
    public:
        Base(int i): b_i_(i)
        {
            cout << "Base(" << b_i_ << ")" << endl;
        }
        ~Base() { cout << "~Base()" << endl; }

        virtual void DummyPrint() { cout << "base:dummy print" << endl; }

        Base& operator=(const Base& b)
        {
            b_i_ = b.b_i_;
            b_i2_ = b.b_i2_;
            cout << "Base operator=()" << endl;
        }

    private:
        int b_i_;
        int b_i2_;
};

class DeriveOne: public virtual Base
{
    public:
        DeriveOne(int i ): Base(i*i), dro_i_(i)
        {
            cout << "DeriveOne(" << dro_i_ << ")" << endl;
        }
        ~DeriveOne() { cout << "~DeriveOne()" << endl; }

        /*
        DeriveOne& operator=(const DeriveOne& d)
        {
            dro_i_ = d.dro_i_;
            cout << "DeriveOne: assignment operator invokes" << endl;
        }
        */

    private:

        int dro_i_;
};

class DeriveTwo: public virtual Base
{
    public:
        DeriveTwo(int i ): Base(i*i + 1), drt_i_(i)
        {
            cout << "DeriveTwo(" << drt_i_ << ")" << endl;
        }
        ~DeriveTwo() { cout << "~DeriveTwo()" << endl; }

    private:

        int drt_i_;
};

class DeriveTop: public DeriveOne, public DeriveTwo
{
    public:
        DeriveTop(int i): Base(i), DeriveOne(i+10), DeriveTwo(i+20), dt_i_(i)
        {
            cout << "DeriveTop(" << dt_i_ << ")" << endl;
        }
        ~DeriveTop() { cout << "~DeriveTop()" << endl; }

    private:

        int dt_i_;
};

int main()
{
    DeriveTop t1(23);
    DeriveTop t2(42);

    // call default assignment operator for DeriveTop

    cout << "assignment operator for DeriveTop" << endl;
    t1 = t2;
    return 0;
}

