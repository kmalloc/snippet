#include <vector>
#include <memory>
#include <string>

class foo
{
    public:
        foo(): a_(42), b_(false), c_(12.2), d_("ssss") {}

    private:
        int a_;
        bool b_;
        double c_;
        std::string d_;
};

static const int g_loop_no = 10000000;

int main()
{
    std::vector<std::shared_ptr<foo>> res;
    res.reserve(g_loop_no);

    for (auto i = 0; i < g_loop_no; ++i)
    {
#ifdef USE_MAKE_SHARED
        auto ptr = std::make_shared<foo>();
#else
        auto ptr = std::shared_ptr<foo>(new foo());
#endif

        res.push_back(ptr);
    }

    return 0;
}

