#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "safe_popen.h"
#include "check_getpwuid.h"

static int int_cmp(const void* v1, const void* v2) {
    return *(int*)v1 - *(int*)v2;
}

static int int_bin_search(int* arr, int sz, int value) {
    if (sz <= 0) return -1;

    int l = 0, h = sz - 1;
    while (l <= h) {
        int m = (l + h)/2;
        if (arr[m] == value) return m;

        if (arr[m] > value) h = m - 1;
        if (arr[m] < value) l = m + 1;
    }

    return -1;
}

static int get_non_local_uid() {
    char line[512];

    FILE* fp = fopen("/etc/passwd", "r");
    if (!fp) return -1;

    int total = 0;
    while (fgets(line, sizeof(line), fp)) {
        ++total;
    }

    rewind(fp);

    int uids[total];

    int i = 0;
    while (i < total && fgets(line, sizeof(line), fp)) {
        char *pp;
        const char *name = strtok_r(line, ":", &pp);
        if (!name) continue;

        // password
        if (!strtok_r(0, ":", &pp)) continue;

        const char *str_uid = strtok_r(0, ":", &pp);
        if (!str_uid) continue;

        uids[++i] = atoi(str_uid);
    }

    fclose(fp);
    if (i <= 0) return -1;

    qsort(uids, i, sizeof(int), int_cmp);

    int k = 0;
    for (; k < 1000000; ++k) {
        int r = int_bin_search(uids, i, k);
        if (r < 0) return k;
    }

    return -1;
}

static int s_support = 0;
void reset_getpwuid_state() {
    s_support = 0;
}

int is_support_getpwuid() {
    if (s_support) return s_support;

    int pfd[2];
    if (pipe(pfd) < 0) return 1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return 2;
    }

    if (pid == 0) {
        struct rlimit lmt;
        lmt.rlim_cur = 0;
        lmt.rlim_max = 0;
        if (setrlimit(RLIMIT_CORE, &lmt) < 0) exit(-1);

        const unsigned int uid = ~0; // get_non_local_uid();
        struct passwd *pwd = getpwuid(uid);
        printf("get pwd for uid %u:%p\n", uid, pwd);

        close(pfd[0]);
        while (write(pfd[1], "done", 5) < 0 && errno == EINTR);

        close(pfd[1]);
        exit(0);
    }

    close(pfd[1]);
    char buff[64] = {0};
    while (read(pfd[0], buff, 5) < 0 && errno == EINTR);

    while (waitpid(pid, 0, 0) < 0) {
        if (errno != EINTR) {
            printf("waitpid for getpwuid child process failed.\n");
            break;
        }
    }

    close(pfd[0]);
    s_support = (strcmp("done", buff) == 0) ? 1 : -1;
    return s_support;
}

int get_username_by_uid_from_getent(const unsigned int uid, char* buff, int sz) {
    char cmd[512];
    if (snprintf(cmd, sizeof(cmd), "getent passwd %u", uid) < 0) return -1;

    int ret = safe_popen_read(cmd, buff, sz, 800);
    if (ret <= 0) return 0;

    char *pp;
    const char *name = strtok_r(buff, ":", &pp);
    if (!name) return 0;

    // password
    if (!strtok_r(0, ":", &pp)) return 0;

    const char *str_uid = strtok_r(0, ":", &pp);
    if (!str_uid || atoi(str_uid) != (int)uid) return 0;

    return strlen(name);
}

int get_username_by_uid(const unsigned int uid, char* buff, int sz) {
    if (sz <= 1) return 0;

    buff[0] = 0;
    if (is_support_getpwuid() > 0) {
        struct passwd *ent = getpwuid(uid);
        if (!ent) {
            return 0;
        }

        int ret = snprintf(buff, sz, "%s", ent->pw_name);
        return ret >= sz? sz - 1:ret;
    } else {
        return get_username_by_uid_from_getent(uid, buff, sz);
    }
}
