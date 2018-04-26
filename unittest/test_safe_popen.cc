#include "gtest/gtest.h"

#include "safe_popen.h"

TEST(common_util_ut, test_safe_popen) {
    char buff[32];

    ASSERT_EQ(safe_popen_read("printf xxxxtestyyyy", buff, sizeof(buff), 800), 12);
    ASSERT_STREQ(buff, "xxxxtestyyyy");
    ASSERT_EQ(safe_popen_read("sleep 1;printf xxxxtestyyyy", buff, sizeof(buff), 800), 0);
    ASSERT_STREQ(buff, "");
    ASSERT_EQ(safe_popen_read("sleep 1;printf xxxxtestyyyy", buff, sizeof(buff), 2000), 12);
    ASSERT_STREQ(buff, "xxxxtestyyyy");

    ASSERT_EQ(safe_popen_read("sleep 1", buff, sizeof(buff), 2000), 0);
    ASSERT_STREQ(buff, "");
    ASSERT_EQ(safe_popen_read("printf 0123456789012345678901234567890123456", buff, sizeof(buff), 800), sizeof(buff) - 1);
    ASSERT_STREQ(buff, "0123456789012345678901234567890");

    safe_popen_read("printf wwwww;sleep 1;printf vvvvv;sleep 1;printf ggggg;sleep 1;printf hhhhh", buff, sizeof(buff), 2000);
    ASSERT_STREQ(buff, "wwwwwvvvvvggggg");
}

struct fork_read_arg_t {
    int do_print;
    int sleep_cnt;
    const char* msg;
};

static void test_fork_read(void* arg) {
    fork_read_arg_t* ap = reinterpret_cast<fork_read_arg_t*>(arg);

    if (ap->sleep_cnt) sleep(ap->sleep_cnt);
    if (ap->do_print) printf("from_test_fork:%s", ap->msg);
}

TEST(common_util_ut, test_fork_read) {
    char buff[32];

    fork_read_arg_t arg;
    arg.msg = "hello";
    arg.do_print = 1;
    arg.sleep_cnt = 0;

    ASSERT_EQ(safe_fork_read(test_fork_read, &arg, buff, sizeof(buff), 800), 20);
    ASSERT_STREQ(buff, "from_test_fork:hello");

    arg.msg = "hello world";
    arg.do_print = 1;
    arg.sleep_cnt = 2;

    ASSERT_EQ(safe_fork_read(test_fork_read, &arg, buff, sizeof(buff), 800), 0);
    ASSERT_STREQ(buff, "");

    ASSERT_EQ(safe_fork_read(test_fork_read, &arg, buff, sizeof(buff), 2800), 26);
    ASSERT_STREQ(buff, "from_test_fork:hello world");

    arg.do_print = 0;
    arg.sleep_cnt = 0;
    ASSERT_EQ(safe_fork_read(test_fork_read, &arg, buff, sizeof(buff), 2800), 0);
    ASSERT_STREQ(buff, "");

    arg.msg = "hello12345678901234567890";
    arg.do_print = 1;
    arg.sleep_cnt = 0;

    ASSERT_EQ(safe_fork_read(test_fork_read, &arg, buff, sizeof(buff), 800), sizeof(buff) - 1);
    ASSERT_STREQ(buff, "from_test_fork:hello12345678901");
}

