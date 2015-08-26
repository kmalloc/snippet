#include <iostream>
using namespace std;

template<typename T, typename S> struct MatchType;

template<typename T>
struct MatchType<T, T>
{
    enum { value = true };
};

template <typename T>
struct IsMatchExist
{
    typedef char small;
    struct big { char d[2]; };

    template<typename T2> static big  test(...);
    template<typename T2> static small test(typename T2::template Match<int>*);

    enum { value = sizeof(test<T>(0)) == sizeof(small) };
};

template <typename T1, typename T2, typename D = void>
struct CheckMatch
{
    enum { value = false };
};

template <typename T1, typename T2>
struct CheckMatch<T1, T2, typename std::enable_if<IsMatchExist<T1>::value>::type>
{
    enum { value = T1::template Match<T2>::value };
};

template <typename T1, typename T2>
struct CheckMatch<T1, T2,
    typename std::enable_if<IsMatchExist<T2>::value && !IsMatchExist<T1>::value>::type>
{
    enum { value = T2::template Match<T1>::value };
};

template <typename T1, typename T2>
struct MatchType
{
    enum { value = CheckMatch<T1, T2>::value };
};

// define an or construct, similar construct should be defined likewise
// user defined construct should contain a template class named Match
// TODO, and construct
template<bool b1, bool b2>
struct EvalOr
{
    enum { value = b2 };
};

template<bool b>
struct EvalOr<true, b>
{
    enum { value = true };
};

template<typename L, typename R>
struct OrType
{
    template<typename T>
    struct Match
    {
        enum { value = EvalOr<MatchType<L, T>::value,
            MatchType<R, T>::value>::value };
    };
};


// test data, tree base construct
struct test {};

template <typename T>
struct wrap {};

template <typename T1, typename T2>
struct wrap2 {};

// be careful not to define those recursive construct like left recursive
struct grammar
    : public OrType<
      OrType<test, wrap<grammar>>,
      OrType<int, wrap2<wrap<grammar>, wrap2<double, char*>>>
      >
{
};

int main()
{
    bool bm = MatchType<bool, bool>::value;
    cout << "bool vs bool:" << bm << endl;

    bm = MatchType<char, bool>::value;
    cout << "char vs bool:" << bm << endl;

    cout << "is grammar has match:" << IsMatchExist<grammar>::value << endl;
    cout << "CheckMatch for <grammar, test>:" << CheckMatch<OrType<OrType<int, test>, OrType<double, test>>, test>::value << endl;

    cout << "is or construct has match, expect:true, actual:" <<
        IsMatchExist<OrType<int, OrType<double, int>>>::value << endl;

    bm = MatchType<grammar, test>::value;
    cout << "grammar1 test, expect:true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, char*>>>,
        test>::value;
    cout << "grammar2 test, expect:true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, char*>>>,
        OrType<int, int>
            >::value;
    cout << "grammar3 test, expect: true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, char*>>>,
        wrap2<int, int>
            >::value;
    cout << "grammar4 test, expect: false, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<double, const char*>
            >::value;
    cout << "grammar5 test, expect:true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<int, OrType<int, const char*>>
            >::value;
    cout << "grammar6 test, expect:true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        OrType<int, OrType<double, const char*>>
            >::value;
    cout << "grammar7 test, expect: true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        wrap2<double, char*>
            >::value;
    cout << "grammar8 test, expect: false, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, wrap2<double, char*>>>,
        wrap2<double, char*>
            >::value;
    cout << "grammar9 test, expect: true, actual:" << bm << endl;

    bm = MatchType<
        OrType<OrType<test, grammar>, OrType<int, OrType<double, const char*>>>,
        wrap2<int, char*>
            >::value;
    cout << "grammar9 test, expect: false, actual:" << bm << endl;

    return 0;
}

