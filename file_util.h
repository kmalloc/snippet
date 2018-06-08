#ifndef __COMMON_FILE_UTIL_H__
#define __COMMON_FILE_UTIL_H__

#include <utime.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <string>
#include <stdio.h>
#include <unistd.h>

inline int create_path(const std::string& path) {
    std::string tp = path;
    size_t sp = tp.find_last_of('/');
    if (sp == std::string::npos) return -1;

    if (sp < tp.size() - 1) tp[sp + 1] = 0;

    if (!access(tp.c_str(), F_OK)) return 0;

    for (size_t i = 1; i <= sp; ++i) {
        if (tp[i] != '/') continue;

        tp[i] = 0;
        if (access(tp.c_str(), F_OK) && mkdir(tp.c_str(), 0755)) return -2;

        tp[i] = '/';
    }

    return 0;
}

inline int copy_file(const std::string& from, const std::string& to, bool preserve) {
    int ifd = -1, ofd = -1;
    if ((ifd = open(from.c_str(), O_RDONLY)) < 0) {
        return -2;
    }

    if ((ofd = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 644)) < 0) {
        close(ifd);
        return -3;
    }

    int ret = -1;
    struct stat st = {0};
    if (!fstat(ifd, &st)) {
        // sendfile support from 2.6.32
        /*
        off_t off = 0;
        ret = sendfile(ofd, ifd, &off, st.st_size);
        */
        int sz = 0;
        char buff[16*1024];

        int total = 0;
        while ((sz = read(ifd, buff, sizeof(buff))) > 0 || (sz < 0 && errno == EINTR)) {
            if (sz < 0) continue; // EINTR

            total += sz;

            int sz2;
            while ((sz2 = write(ofd, buff, sz)) < 0 && errno == EINTR) continue;

            if (sz2 <= 0) break;
        }

        if (total == st.st_size) ret = 1;
    }

    if (preserve && ret > 0) {
        /*
        struct timespec tm[2];
        tm[0] = st.st_atim;
        tm[1] = st.st_mtim;

        if (utimensat(0, to.c_str(), tm, 0)) return -4;
        */

        struct timeval tm[2];
        tm[0].tv_sec = st.st_atim.tv_sec;
        tm[0].tv_usec = st.st_atim.tv_nsec/1000;

        tm[1].tv_sec = st.st_mtim.tv_sec;
        tm[1].tv_usec = st.st_mtim.tv_nsec/1000;

        if (utimes(to.c_str(), tm)) ret = -4;
        else if (fchmod(ofd, st.st_mode)) ret = -5;
        else if (fchown(ofd, st.st_uid, st.st_gid)) ret = -6;
    }

    if (ifd >= 0) close(ifd);
    if (ofd >= 0) close(ofd);

    return ret;
}

#endif

