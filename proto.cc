#include <iostream>
using namespace std;

template<typename T, typename S>
struct MatchType
{
    enum { value = false };
};

template<typename T>
struct MatchType<T, T>
{
    enum { value = true };
};

template<typename L, typename R>
struct OrType
{

};

template <bool b1, bool b2>
struct EvalOr
{
    enum { value = b2 };
};

template <bool b>
struct EvalOr<true, b>
{
    enum { value = true };
};

template <typename T1, typename T2, typename T3>
struct MatchType<OrType<T1, T2>, T3>
{
    enum { value = EvalOr<MatchType<T1, T3>::value,
        MatchType<T2, T3>::value>::value };
};

template <typename T1, typename T2>
struct MatchType<OrType<T1, T2>, OrType<T1, T2>>
{
    enum { value = true };
};

struct test {};

struct grammar
    : public OrType<
      OrType<test, grammar>,
      OrType<int, OrType<double, char*>>
      >
{
};

int main()
{
    bool bm = MatchType<bool, bool>::value;
    cout << "bool vs bool:" << bm << endl;

    bm = MatchType<char, bool>::value;
    cout << "char vs bool:" << bm << endl;

    test t;
    bm = MatchType<grammar, test>::value;
    cout << "grammar vs t:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, char*>>>,
        test>::value;
    cout << "grammar2 vs t:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, char*>>>,
        OrType<int, int>
            >::value;
    cout << "grammar3 vs t:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<double, const char*>
            >::value;
    cout << "grammar4 vs t:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<int, OrType<int, const char*>>
            >::value;
    cout << "grammar5 vs t:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<int, OrType<double, const char*>>
            >::value;
    cout << "grammar6 vs t:" << bm << endl;

    return 0;
}

