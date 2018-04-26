#include <time.h>
#include <stdio.h>
#include <sys/time.h>

static int get_precise_tm(char* buff, int size) {
    struct timeval cur_tm;
    gettimeofday(&cur_tm, NULL);

    int sz = strftime(buff, size, "%Y-%m-%d_%H:%M:%S", localtime(&cur_tm.tv_sec));
    if (sz <= 0 || sz > size) return -1;

    int sz2 = snprintf(buff + sz, size - sz, ".%ld", cur_tm.tv_usec);
    if (sz2 < 0) return -2;

    return sz + sz2;
}

static int read_sched_pid(int pid) {
    char path[128];

    if (pid < 0) return -1;

    path[0] = 0;
    if (pid == 0) {
        snprintf(path, sizeof(path), "/proc/self/sched");
    } else {
        snprintf(path, sizeof(path), "/proc/%d/sched", pid);
    }

    char buff[256];

    FILE* fp = fopen(path, "r");
    if (!fp) return -1;

    char* ret = fgets(buff, sizeof(buff), fp);
    fclose(fp);
    if (!ret) return -2;

    char* p = buff;
    while (*p && *p != '(') ++p;
    while (*p && (*p < '0' || *p > '9')) ++p;

    int v = 0;
    while (*p && *p >= '0' && *p <= '9') {
        v = v * 10 + *p - '0';
        ++p;
    }

    return v;
}

int main() {
    char buff[32];

    get_precise_tm(buff, sizeof(buff));
    printf("sched pid of 1:%d, cur time:%s\n", read_sched_pid(1), buff);

    get_precise_tm(buff, sizeof(buff));
    printf("sched pid of 4389:%d, cur time:%s\n", read_sched_pid(4389), buff);
}

