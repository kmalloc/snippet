#include <iostream>
using namespace std;

template<typename T, typename S> struct MatchType;

template<typename T>
struct MatchType<T, T>
{
    enum { value = true };
};

template<typename T>
struct IsMatchExist
{
    typedef char small;
    struct big { char d[2]; };

    template<typename T2> static big  test(...);
    template<typename T2> static small test(typename T2::template Match<int>*);

    enum { value = sizeof(test<T>(0)) == sizeof(small) };
};

template<typename T>
struct HasResult
{
    typedef char small;
    struct big { char d[2]; };

    template<typename T2> static big  test(...);
    template<typename T2> static small test(typename T2::result*);

    enum { value = sizeof(test<T>(0)) == sizeof(small) };
};

//primary definition
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

// following is not really necessary for matching type,
// since usually it is true to have one type matchs another,
// but the reverse is not required at the same time,
// here just implement it for fun and for testing.
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

template<bool b1, typename T1, bool b2, typename T2> struct TypeSelector;

template<typename T1, bool b2, typename T2>
struct TypeSelector<true, T1, b2, T2>
{
    typedef T1 type;
};

template<typename T1, typename T2>
struct TypeSelector<false, T1, true, T2>
{
    typedef T2 type;
};

// declaration
template<typename T, typename D = void> struct ResultOf;

template<typename T>
struct ResultOf<T, typename std::enable_if<HasResult<T>::value>::type>
{
    typedef typename T::result type;
};

// define an or construct, similar construct should be defined likewise
// user defined construct should contain a template class named Match
// TODO, and construct
template<typename L, typename R>
struct OrType
{
    template<typename T>
    struct Match
    {
        enum { value = MatchType<L, T>::value || MatchType<R, T>::value };
    };

    template<typename T, typename D=void> struct Evaluator;

    template<typename T>
    struct Evaluator<T, typename std::enable_if<MatchType<L, T>::value>::type>
    {
        static typename ResultOf<L>::type Eval(const T& exp)
        {
            return L()(exp);
        }
    };

    template<typename T>
    struct Evaluator<T, typename std::enable_if<MatchType<R, T>::value>::type>
    {
        static typename ResultOf<R>::type Eval(const T& exp)
        {
            return R()(exp);
        }
    };

    template<typename T>
    typename ResultOf<typename
    TypeSelector<MatchType<L, T>::value, L, MatchType<R, T>::value, R>::type
    >::type operator()(const T& exp)
    {
        return Evaluator<T>::Eval(exp);
    }
};


// test data, tree base construct
struct test
{
    typedef void* result;

    template<typename E>
    result operator()(const E& exp) { return reinterpret_cast<void*>(42); }
};

struct test2
{
    typedef int result;

    template<typename E>
    result operator()(const E& exp) { return 0x200; }
};

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

    bm = CheckMatch<OrType<OrType<int, test>, OrType<double, test>>, test>::value;
    cout << "CheckMatch for <grammar, test>:" << bm << endl;

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

    bm = MatchType<OrType<test, test2>, test>::value;
    cout << "grammar9 test, expect: true, actual:" << bm << endl;

    cout << "is struct test has result type, expected:true, actual:"
        << HasResult<test>::value << endl;

    cout << "is struct test2 has result type, expected:true, actual:"
        << HasResult<test2>::value << endl;

    auto res = OrType<test, test2>()(test());
    cout << "evaluating test from OrType<test, test2>, expected:42, actual:"
        << res << endl;

    auto res2 = OrType<test, test2>()(test2());
    cout << "evaluating test2 from OrType<test, test2>, expected:0x200, actual:"
        << hex << res2 << endl;

    return 0;
}

