#include <iostream>
using namespace std;

// declaration.
template<class T> class tDimen
{
    public:
        enum { dim = 0; }
        typedef T ArrType;
};

template<class T, int SZ1, int ...SZ>
class tDimen
{
    public:
        enum { dim = 1 + tDimen<T, SZ...>::dim; }
        typedef tDimen<T, SZ...>::Type ArrType[SZ1];

    private:
        ArrType arr_;
};

template<class T, int ...SZ>
class tDimen<tDimen<T, SZ...>>
{
    public:
        enum { dim = 1 + tDimen<T, SZ...>::dim; }
        typedef vector<tDimen<T, SZ...>::ArrType> ArrType;

    private:
        ArrType arr_;
};

template<class T>
class tDimen<tDimen<T>>
{
    public:
        enum { dim = 1 + tDimen<T>::dim; }
        typedef vector<tDimen<T>::ArrType> ArrType;

    private:
        ArrType arr_;
};


template<class T, int N>
CalcDim(const T (&arr)[N])
{
    cout << "arr:" << N << endl;
}

template<class T>
CalcDim(const T* pt)
{
    cout << "pointer:" << endl;
}

int main()
{
    int int_arr[3];

    tDimen<int, 2, 3, 4> 3d;
}

