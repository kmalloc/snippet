#include "gtest/gtest.h"

#include "safe_popen.h"
#include "check_getpwuid.h"

#include <dlfcn.h>

typedef struct passwd* (*getpwuid_func)(uid_t uid);

static int g_getpwuid_ct = 0;
static bool g_crash_getpwuid = false;

extern "C" struct passwd* getpwuid(uid_t uid) {
    static getpwuid_func orig_func = (getpwuid_func)dlsym(RTLD_NEXT, "getpwuid");
    if (g_crash_getpwuid) abort();

    ++g_getpwuid_ct;
    return orig_func(uid);
}

TEST(common_util_ut, test_getpwuid) {
    char buff[64];
    ASSERT_EQ(4, get_username_by_uid(0, buff, sizeof(buff)));
    ASSERT_STREQ("root", buff);
    ASSERT_EQ(1, g_getpwuid_ct);

    reset_getpwuid_state();
    g_crash_getpwuid = true;
    ASSERT_EQ(4, get_username_by_uid(0, buff, sizeof(buff)));
    ASSERT_STREQ("root", buff);
    ASSERT_EQ(1, g_getpwuid_ct);

    ASSERT_EQ(4, get_username_by_uid_from_getent(0, buff, sizeof(buff)));
    ASSERT_STREQ("root", buff);

    reset_getpwuid_state();
    g_crash_getpwuid = false;
    ASSERT_EQ(0, get_username_by_uid(~0, buff, sizeof(buff)));
    ASSERT_STREQ("", buff);
    ASSERT_EQ(2, g_getpwuid_ct);

    reset_getpwuid_state();
    g_crash_getpwuid = true;
    ASSERT_EQ(0, get_username_by_uid(~0, buff, sizeof(buff)));
    ASSERT_STREQ("", buff);
    ASSERT_EQ(2, g_getpwuid_ct);

    ASSERT_EQ(0, get_username_by_uid_from_getent(~0, buff, sizeof(buff)));
    ASSERT_STREQ("", buff);

    char pwd_line[256];
    ASSERT_TRUE(safe_popen_read("cat /etc/passwd|tail -n 1", pwd_line, sizeof(pwd_line), 800) > 0);

    char* pp;
    char* un1 = strtok_r(pwd_line, ":", &pp);
    ASSERT_TRUE(un1);
    ASSERT_TRUE(strtok_r(0, ":", &pp));

    char* uid1 = strtok_r(0, ":", &pp);
    ASSERT_TRUE(uid1);

    int id1 = atoi(uid1);
    ASSERT_TRUE(id1 >= 0);

    int un1_sz = strlen(un1);
    reset_getpwuid_state();
    g_crash_getpwuid = false;
    ASSERT_EQ(un1_sz, get_username_by_uid(id1, buff, sizeof(buff)));
    ASSERT_STREQ(un1, buff);
    ASSERT_EQ(3, g_getpwuid_ct);

    reset_getpwuid_state();
    g_crash_getpwuid = true;
    ASSERT_EQ(un1_sz, get_username_by_uid(id1, buff, sizeof(buff)));
    ASSERT_STREQ(un1, buff);
    ASSERT_EQ(3, g_getpwuid_ct);

    ASSERT_EQ(un1_sz, get_username_by_uid_from_getent(id1, buff, sizeof(buff)));
    ASSERT_STREQ(un1, buff);
}
