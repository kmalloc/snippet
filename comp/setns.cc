#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>

#include <dirent.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

#include <stdio.h>
#include <pcap.h>

#if defined(__x86_64__)
#define SETNS_SYSCALL_NO 308
#elif defined(__i386__)
#define SETNS_SYSCALL_NO 346
#endif

static int enter_proc_ns(const char* pid, int type)  {
    int ori_fd = -1;
    char proc_ns_path[256] = {0};
    if (type == CLONE_NEWNS) {
        ori_fd = open("/proc/self/ns/mnt", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/mnt", pid);
    } else if (type == CLONE_NEWIPC) {
        ori_fd = open("/proc/self/ns/ipc", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/ipc", pid);
    } else if (type == CLONE_NEWNET) {
        ori_fd = open("/proc/self/ns/net", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/net", pid);
    } else if (type == CLONE_NEWPID) {
        ori_fd = open("/proc/self/ns/pid", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/pid", pid);
    } else {
        printf("unsupported type\n");
        return -1;
    }

    if (ori_fd < 0) {
        printf("opening current process namespace fails\n");
        return -1;
    }

    printf("process namespace path:%s\n", proc_ns_path);

    int ns_fd = open(proc_ns_path, O_RDONLY);
    if (ns_fd < 0) {
        printf("open target process ns failed\n");
        close(ori_fd);
        return -1;
    }

    printf("\nnow entering namespace of process %s\n", pid);

    // setns() system call, supported from linux 3.0, but not available in some glibc yet.
    // docker also relys on this system, wherever docker runs, this system call is available.
    int ret = syscall(SETNS_SYSCALL_NO, ns_fd, type);
    if (ret < 0) {
        printf("setns failed\n");
        close(ori_fd);
        return -1;
    }

    close(ns_fd);
    return ori_fd;
}

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    printf("Jacked a packet with length of [%d]\n", header->len);
}

pcap_t* setup_pcap(const char* dev) {
    pcap_t *handle;         /* Session handle */
    char errbuf[PCAP_ERRBUF_SIZE];  /* Error string */
    struct bpf_program fp;      /* The compiled filter */
    char filter_exp[] = "port 36000 and src 10.220.0.202";  /* The filter expression */
    bpf_u_int32 mask;       /* Our netmask */
    bpf_u_int32 net;        /* Our IP */
    struct pcap_pkthdr header;  /* The header that pcap gives us */
    const u_char *packet;       /* The actual packet */

    /* Define the device */
    if (!dev) dev = pcap_lookupdev(errbuf);

    if (dev == NULL) {
        fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
        return 0;
    }

    /* Find the properties for the device */
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n", dev, errbuf);
        net = 0;
        mask = 0;
    }
    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return 0;
    }
    /* Compile and apply the filter */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_close(handle);
        return 0;
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_close(handle);
        return 0;
    }

    return handle;
}

/* Read all available inotify events from the file descriptor 'fd'.
   wd is the table of watch descriptors for the directories in argv.
   argc is the length of wd and argv.
   argv is the list of watched directories.
   Entry 0 of wd and argv is unused.  */

struct result_t {
    int fd;
    int* wd;
};

static void handle_events(int fd, int *wd, const int argc, const char* argv[]) {
    /* Some systems cannot read integer variables if they are not
       properly aligned. On other systems, incorrect alignment may
       decrease performance. Hence, the buffer used for reading from
       the inotify file descriptor should have the same alignment as
       struct inotify_event. */

    int i;
    ssize_t len;
    char *ptr;
    const struct inotify_event *event;
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    /* Loop while events can be read from inotify file descriptor. */

    for (;;) {

        /* Read some events. */

        len = read(fd, buf, sizeof buf);
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* If the nonblocking read() found no events to read, then
           it returns -1 with errno set to EAGAIN. In that case,
           we exit the loop. */

        if (len <= 0)
            break;

        /* Loop over all events in the buffer */

        for (ptr = buf; ptr < buf + len;
                ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            /* Print event type */

            if (event->mask & IN_OPEN)
                printf("IN_OPEN: ");
            if (event->mask & IN_CLOSE_NOWRITE)
                printf("IN_CLOSE_NOWRITE: ");
            if (event->mask & IN_CLOSE_WRITE)
                printf("IN_CLOSE_WRITE: ");
            if (event->mask & IN_CREATE)
                printf("IN_CREATE:");
            if (event->mask & IN_DELETE)
                printf("IN_DELETE:");
            if (event->mask & IN_MODIFY)
                printf("IN_MODIFY:");

            /* Print the name of the watched directory */

            for (i = 0; i < argc; ++i) {
                if (wd[i] == event->wd) {
                    printf("%s/", argv[i]);
                    break;
                }
            }

            /* Print the name of the file */

            if (event->len)
                printf("%s", event->name);

            /* Print type of filesystem object */

            if (event->mask & IN_ISDIR)
                printf(" [directory]\n");
            else
                printf(" [file]\n");
        }
    }
}

static void  wait_event(int fd, int* wd, unsigned int argc, const char* argv[]) {

    struct pollfd fds[2];
    nfds_t nfds = 2;

    /* Console input */

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    /* Inotify input */

    fds[1].fd = fd;
    fds[1].events = POLLIN;

    /* Wait for events and/or terminal input */

    printf("Listening for events.\n");
    while (1) {
        int poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("error occurs, poll");
            break;
        }

        if (poll_num > 0) {

            if (fds[0].revents & POLLIN) {

                /* Console input is available. Empty stdin and quit */
                char buf;
                while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                    continue;
                break;
            }

            if (fds[1].revents & POLLIN) {

                /* Inotify events are available */

                handle_events(fd, wd, argc, argv);
            }
        }
    }

    printf("Listening for events stopped.\n");
}


struct result_t add_inotify(unsigned int argc, const char* argv[]) {
    struct result_t ret = {-1, 0};

    /* Create the file descriptor for accessing the inotify API */

    ret.fd = inotify_init1(IN_NONBLOCK);
    if (ret.fd == -1) {
        perror("inotify_init1");
        return ret;
    }

    /* Allocate memory for watch descriptors */

    ret.wd = (int*)calloc(argc, sizeof(int));
    if (ret.wd == NULL) {
        perror("calloc");
        return ret;
    }

    /* Mark directories for events
       - file was opened
       - file was closed */

    for (unsigned int i = 0; i < argc; i++) {
        ret.wd[i] = inotify_add_watch(ret.fd, argv[i], IN_OPEN | IN_CLOSE | IN_CREATE | IN_DELETE | IN_MODIFY);
        if (ret.wd[i] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", argv[i]);
            perror("inotify_add_watch");
            continue;
        }
    }

    return ret;
}


static void run_inotify(const char* path) {
    struct result_t rn = add_inotify(1, &path);

    wait_event(rn.fd, rn.wd, 1, &path);

    if (rn.wd) {
        close(rn.fd);
        free(rn.wd);
    }
}

void scan_process(const std::string& path = "/proc") {
    DIR *dp = opendir(path.c_str());
    if (!dp) return;

    struct dirent *dirp;

    while ((dirp = readdir(dp))) {
        // skip non-numeric entries
        int id = atoi(dirp->d_name);
        if (id <= 0) continue;

       printf("find pid in /proc:%s\n", dirp->d_name);
    }

    closedir(dp);
}

static void mnt_op(const char* pid, bool back) {
    int ori_fd = enter_proc_ns(pid, CLONE_NEWNS);
    if (ori_fd < 0) return;

    printf("exec cmd:'pwd;whoami;cd /home/visitor_from_host;pwd;ls'\n");
    int ret = system("/bin/bash -c 'pwd;whoami;cd /home/visitor_from_host;pwd;ls'");
    printf("exit status of system:%x\n", ret);

    const char* path[] = {"/root"};
    const unsigned int sz = sizeof path/sizeof path[0];

    struct result_t rn = add_inotify(sz, path);

    scan_process();

    if (back) { 
        printf("now return to original namespace, exec cmd:pwd;whoami;cd ~;pwd;ls\n");

        ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWNS);
        if (ret < 0) {
            printf("getting back to origin namespace fails\n");
            close(ori_fd);
            return;
        }

        ret = system("/bin/bash -c 'pwd;whoami;cd ~;pwd;ls'");
    }

    printf("\nstart to wait for inotify event, press ENTER key to terminate.\n");

    wait_event(rn.fd, rn.wd, sz, path);

    if (rn.wd) {
        close(rn.fd);
        free(rn.wd);
    }

    close(ori_fd);
}

static void* create_shm(size_t key, bool create, int* out_id = NULL) {
    int id = shmget(key, 32, 0666);
    if (id == -1 && !create) return 0;

    if (id == -1) {
        id = shmget(key, 32, 0666|IPC_CREAT|IPC_EXCL);
        if (id == -1) return 0;
    }

    if (out_id) *out_id = id;

    return shmat(id, NULL, 0);
}

static void ipc_op(char* type, const char* pid) {
    int ori_fd = -1;
    const size_t key = 0x23342422;
    if (pid) {
        ori_fd = enter_proc_ns(pid, CLONE_NEWIPC);
    }

    if (ori_fd < 0) {
        printf("entering target ns failed for ipc_op\n");
        return;
    }

    if (!strcmp(type, "create")) {
        int* p = (int*)create_shm(key, true);

        int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWIPC);
        close(ori_fd);

        *p = key;
        printf("create shm done, value:%zx\n", key);
    } else if (!strcmp(type, "read")) {
        int* p = (int*)create_shm(key, false);
        int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWIPC);
        close(ori_fd);

        if (!p) {
            printf("get shm for read failed\n");
            return;
        }
        printf("get shm for read succ, value:%d, key:0x%zx\n", *p, key);
    } else if (!strcmp(type, "write")) {
        int* p = (int*)create_shm(key, false);
        int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWIPC);
        close(ori_fd);

        if (!p) {
            printf("get shm for write failed\n");
            return;
        }

        *p = atoi(pid);
        printf("get shm for write succ, value:%d, key:0x%zx\n", *p, key);
    } else if (!strcmp(type, "del")) {
        int id = -1;
        int* p = (int*)create_shm(key, false, &id);

        printf("get shm for del succ, before deleting value:%d, key:0x%zx\n", *p, key);

        if (shmdt(p) < 0) printf("shmdt failed for deleting shm in ns\n");
        if (shmctl(id, IPC_RMID, NULL) < 0) printf("shmctl for deleting mem failed\n");

        int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWIPC);
        close(ori_fd);

        if (!p) {
            printf("get shm for del failed\n");
            return;
        }

    }
}

static void cgroup_op(const char* cg_file) {
    if (cg_file) {
        int fd = open(cg_file, O_RDWR);
        if (fd < 0) {
            printf("open %s failes\n", cg_file);
            return;
        }

        if (write(fd, "0", 1) <= 0) {
            printf("writting to %s fails\n", cg_file);
            close(fd);
            return;
        }

        close(fd);
    }

    while (1) {}
}

static size_t split(const std::string& input,
        char delim, std::vector<std::string>& output) {
    int c = 0;
    std::string item;
    std::stringstream ss;

    ss.str(input);
    while (std::getline(ss, item, delim)) {
        c++;
        output.push_back(item);
    }

    return c;
}

static std::string get_cgroup_subroot(const std::string& type) {
    std::string s;
    std::ifstream fin("/proc/self/mountinfo");

    std::vector<std::string> res, res2;

    res.reserve(16);
    res2.reserve(8);

    while (std::getline(fin, s)) {
        res.clear();
        split(s, ' ', res);        
        if (res.size() < 4 && res[res.size() - 3] == "cgroup") continue;

        res2.clear();
        split(res[res.size() - 1], ',', res2);

        for (size_t i = 0; i < res2.size(); ++i) {
            if (res2[i] != type) continue;

            return res[4];
        }
    }

    return "";
}

static std::string dir_of(const std::string& path) {
    size_t last = path.size();
    if (!last) return "";

    if (path[last - 1] == '/') --last;

    while (last > 0 && path[last - 1] != '/') {
        --last; 
    }

    return path.substr(0, last);
}

static std::string get_cgroup_root() {
    return  dir_of(get_cgroup_subroot("cpu"));
}

static void display_cgroup() {
    const char* s1 = "/abc/def/gh/";
    const char* s2 = "/abc/def/gh";
    std::cout << "origin:" << s1 << ", test:" << dir_of(s1) << std::endl;
    std::cout << "origin:" << s2 << ", test:" << dir_of(s2) << std::endl;

    std::cout << "root:" << get_cgroup_root() << std::endl;
    std::cout << "cpu:" << get_cgroup_subroot("cpu") << std::endl;
    std::cout << "cpuset:" << get_cgroup_subroot("cpuset") << std::endl;
    std::cout << "memory:" << get_cgroup_subroot("memory") << std::endl;
    std::cout << "blkio:" << get_cgroup_subroot("blkio") << std::endl;
}

static bool write_to_file(const std::string& file, const char* cont, int sz) {
    if (sz <= 0 || access(file.c_str(), W_OK)) return false;

    FILE* fd = fopen(file.c_str(), "w");
    if (!fd) {
        printf("can not open %s\n", file.c_str());
        return false;
    }

    if (static_cast<size_t>(sz) != fwrite(cont, 1, sz, fd)) {
        fclose(fd);
        printf("write to %s failed\n", file.c_str());
        return false;
    }

    fclose(fd);
    return true;
}

static bool setup_cpu_cgroup(const std::string& root, int ratio) {
    if (ratio > 100) ratio = 100;

    int period = 100000;
    int quota = 100000 * ratio / 100;

    if (quota < 1000) quota = 1000;

    char buf[64];

    int sz = snprintf(buf, sizeof buf, "%d", period);
    if (!sz || !write_to_file(root + "/cpu.cfs_period_us", buf, sz)) return false;

    sz = snprintf(buf, sizeof buf, "%d", quota);
    if (!sz || !write_to_file(root + "/cpu.cfs_quota_us", buf, sz)) return false;

    return true;
}

static bool is_valid_mem_setting(const std::string& mem) {
    if (mem.empty()) return false;

    for (size_t i = 0; i < mem.size() - 1; ++i) {
        if (std::isdigit(mem.at(i))) continue;

        return false;
    }   

    char c = mem.at(mem.size() - 1);
    return std::isdigit(c) || c == 'k' || c == 'K' || c == 'm' || c == 'M' || c == 'g' || c == 'G';
}

static bool setup_mem_cgroup(const std::string& root, const std::string& mem) {
    if (!is_valid_mem_setting(mem)) {
        printf("invalid input for memory setting\n");
        return false;
    }

    long long total = 0;
    char c = mem.at(mem.size() - 1);

    if (c == 'k' || c == 'K') {
        sscanf(mem.c_str(), "%lld%*c", &total);
        total *= 1024;
    } else if (c == 'm' || c == 'M') {
        sscanf(mem.c_str(), "%lld%*c", &total);
        total *= 1024 * 1024;
    } else if (c == 'g' || c == 'G') {
        sscanf(mem.c_str(), "%lld%*c", &total);
        total *= 1024 * 1024 * 1024;
    } else {
        sscanf(mem.c_str(), "%lld", &total);
    }

    if (total <= 0) return false;

    char buf[64];
    int sz = snprintf(buf, sizeof buf, "%lld", total);
    if (!sz || sz != write_to_file(root + "/memory.limit_in_bytes", buf, sz)) return false;

    sz = snprintf(buf, sizeof buf, "%lld", total * 2);
    if (!sz || sz != write_to_file(root + "/memory.memsw.limit_in_bytes", buf, sz)) {
        printf("setting memory.memsw.limit_in_bytes failed, possibly system doesn't support this option\n");
        return true;
    }

    return true;
}

static bool create_onion_cgroup(const std::string& cgroup_root, const std::string& name) {
    if (cgroup_root.empty()) {
        printf("can not find cgroup root\n");
        return false;
    }

    if (access(cgroup_root.c_str(), W_OK)) {
        printf("can not access cgroup root\n");
        return false;
    }

    std::string cpu_root = cgroup_root + "/cpu/" + name;
    std::string mem_root = cgroup_root + "/memory/" + name;

    if (access(cpu_root.c_str(), F_OK)) {
        mkdir(cpu_root.c_str(), 0755);
    }

    if (access(mem_root.c_str(), F_OK)) {
        mkdir(mem_root.c_str(), 0755);
    }

    printf("cgroup for onion(cpu & memory) exits or created successfully\n");
    return true;
}

static void create_cg() {
    display_cgroup();
    std::string cgroup_root = get_cgroup_root();
    if (!create_onion_cgroup(cgroup_root, "onion")) return;

    setup_cpu_cgroup(cgroup_root + "/cpu/onion", 10);
    setup_mem_cgroup(cgroup_root + "/memory/onion", "40m");
}

static int make_socket_non_blocking(int sfd) {
    int flags, s;

    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        return -1;
    }

    return 0;
}

static int epoll_add(int efd, int fd_to_add) {
    struct epoll_event event;

    event.data.fd = fd_to_add;
    event.events = EPOLLIN | EPOLLET;

    return epoll_ctl(efd, EPOLL_CTL_ADD, fd_to_add, &event);
}

static int svr_loop(int tcp_fd, int udp_fd) {
    int efd = epoll_create1(EPOLL_CLOEXEC);
    if (efd < 0) {
        fprintf(stderr, "create epoll fd failed:");
        perror(0);
        return -1;
    }

    if (epoll_add(efd, tcp_fd) < 0) {
        fprintf(stderr, "add tcp fd to epoll failed:");
        perror(0);
        close(efd);
        return -1;
    }

    if (epoll_add(efd, udp_fd) < 0) {
        fprintf(stderr, "add udp fd to epoll failed:");
        perror(0);
        close(efd);
        return -1;
    }

    const int E_MAX_EVENT = 64;
    struct epoll_event income_event[E_MAX_EVENT];

    while (1) {
        int n = epoll_wait(efd, income_event, E_MAX_EVENT, -1);
        for (int i = 0; i < n; i++) {
            if ((income_event[i].events & EPOLLERR) ||
                    (income_event[i].events & EPOLLHUP) ||
                    (!(income_event[i].events & EPOLLIN))) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                fprintf (stderr, "epoll error\n");
                close (income_event[i].data.fd);
                continue;
            } else if (tcp_fd == income_event[i].data.fd) {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    socklen_t in_len;
                    struct sockaddr in_addr;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof in_addr;
                    infd = accept(tcp_fd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            perror ("accept error.");
                            break;
                        }
                    }

                    int s = getnameinfo(&in_addr, in_len,
                            hbuf, sizeof hbuf,
                            sbuf, sizeof sbuf,
                            NI_NUMERICHOST | NI_NUMERICSERV);
                    if (s == 0) {
                        printf("Accepted connection on descriptor %d "
                                "(host=%s, port=%s)\n", infd, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    s = make_socket_non_blocking(infd);
                    if (s == -1) {
                        printf("make socket non blocking failed, fd:%d\n", infd);
                        close(infd);
                        continue;
                    }

                    if (epoll_add(efd, infd) == -1) {
                        printf("epoll add accepted socket failed, fd:%d\n", infd);
                    }
                }
                continue;
            } else {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read(income_event[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN) {
                            printf("read on fd done.\n");
                        }

                        break;
                    } else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }

                    char info[128];
                    sprintf(info, "recv buff from sock:%d:\n", income_event[i].data.fd);

                    /* Write the buffer to standard output */
                    write(1, info, strlen(info));

                    int s = write(1, buf, count);
                    if (s == -1) {
                        done = 1;
                        printf("write to fd failed\n");
                        break;
                    }
                }

                if (done) {
                    printf ("Closed connection on descriptor %d\n", income_event[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close (income_event[i].data.fd);
                }
            }
        }
    }

    close(efd);
    return 0;
}

static int create_svr_socket(int type, const char* addr) {   
    /* First call to socket() function */
    int sock_fd = socket(PF_UNIX, type, 0);

    if (sock_fd < 0) {
        return -1;
    }

    unlink(addr);

    struct sockaddr_un serv_addr;

    /* Initialize socket structure */
    bzero((char *)&serv_addr, sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, addr, strlen(addr));

    /* Now bind the host address using bind() call.*/
    if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        return -1;
    }

    /* Now start listening for the clients, here process will
     * go in sleep mode and will wait for the incoming connection
     */
    if (type == SOCK_STREAM) {
        if (listen(sock_fd,5) < 0) {
            close(sock_fd);
            return -1;
        }
    }

    return sock_fd;
}

static void run_pcap_in_container(const char* pid, const char* dev) {
    if (!pid) {
        printf("please specify pid in container\n");
        return;
    }

    int ori_fd = enter_proc_ns(pid, CLONE_NEWNET);
    if (ori_fd < 0) {
        printf("enter net namespace of pid[%s] failed\n", pid);
        return;
    }

    pcap_t* hd = setup_pcap(dev);
    if (!hd) {
        close(ori_fd);
        printf("setup pcap failed\n");
        return;
    }

    int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWNET);
    if (ret < 0) {
        printf("return to origin net namespace failed\n");
        perror(0);
        close(ori_fd);
        pcap_close(hd);
        return;
    }

    printf("now return to original net namespace,and waiting for packet\n");

    close(ori_fd);
    pcap_loop(hd, 1024*1024, got_packet, 0);
    pcap_close(hd);
}

static int run_socket_from_root(const char* pid, const char* tcp, const char* udp) {
    char tcp_path[256];
    char udp_path[256];

    snprintf(tcp_path, sizeof(tcp_path), "/proc/%s/root/%s", pid, tcp);
    snprintf(udp_path, sizeof(udp_path), "/proc/%s/root/%s", pid, udp);

    int tcp_fd = create_svr_socket(SOCK_STREAM, tcp_path);
    if (tcp_fd < 0) {
        fprintf(stderr, "create tcp socket failed:");
        perror(0);
        return -1;
    }

    int udp_fd = create_svr_socket(SOCK_DGRAM, udp_path);
    if (udp_fd < 0) {
        fprintf(stderr, "create udp socket failed:");
        perror(0);
        close(tcp_fd);
        return -1;
    }

    svr_loop(tcp_fd, udp_fd);
    return 0;
}

static int run_socket_in_ns(const char* pid, const char* tcp, const char* udp) {
    int ori_fd = enter_proc_ns(pid, CLONE_NEWNS);
    if (ori_fd < 0) {
        printf("enter mnt namespace failed\n");
        return -1;
    }

    int tcp_fd = create_svr_socket(SOCK_STREAM, tcp);
    if (tcp_fd < 0) {
        fprintf(stderr, "create tcp socket failed:");
        perror(0);
        return -1;
    }

    int udp_fd = create_svr_socket(SOCK_DGRAM, udp);
    if (udp_fd < 0) {
        fprintf(stderr, "create udp socket failed:");
        perror(0);
        close(tcp_fd);
        return -1;
    }

    int ret = syscall(SETNS_SYSCALL_NO, ori_fd, CLONE_NEWNS);
    if (ret < 0) {
        fprintf(stderr, "return to old namespace failed:");
        perror(0);
        close(tcp_fd);
        close(udp_fd);
        close(ori_fd);
        return -1;
    }

    close(ori_fd);
    svr_loop(tcp_fd, udp_fd);
}

// docker inspect --format {{.State.Pid}} $container_id
// get nmt namespace id from process: /proc/$pid/ns/mnt.
//

extern "C" int nl_main(const char* pid);

int main(int argc, char* argv[]) {
    const static char* usage = "Usage: (cgroot), (pcap pid dev), (cgroup cg_path), (ipc create(or read or del or no) pid(or null)), (mnt pid back(or null)), (socket pid tcp_path udp_path), (socket-from-root pid tcp_path udp_path), (inotify path)\n";
    if (argc <= 1) {
        printf(usage);
    } else if (!strcmp(argv[1], "cgroot")) {
        create_cg();
    } else if (!strcmp(argv[1], "ipc") && argc >= 3) {
        ipc_op(argv[2], argc == 4?argv[3]:NULL);
    } else if (!strcmp(argv[1], "mnt") && argc >= 3) {
        mnt_op(argv[2], argc == 4 && !strcmp(argv[3], "back"));
    } else if (!strcmp(argv[1], "cgroup")) {
        const char* cg = argc >= 3?argv[2]:NULL;
        cgroup_op(cg);
    } else if (!strcmp(argv[1], "pcap")) {
        const char* pid = argv[2];
        const char* dev = argv[3];
        run_pcap_in_container(pid, dev);
    } else if (!strcmp(argv[1], "socket") && argc >= 5) {
        const char* pid = argv[2];
        const char* tcp = argv[3];
        const char* udp = argv[4];
        run_socket_in_ns(pid, tcp, udp);
    } else if (!strcmp(argv[1], "socket-from-root") && argc >= 5) {
        const char* pid = argv[2];
        const char* tcp = argv[3];
        const char* udp = argv[4];
        run_socket_from_root(pid, tcp, udp);
    } else if (!strcmp(argv[1], "inotify") && argc >= 3) {
        run_inotify(argv[2]);
    } else if (!strcmp(argv[1], "nl-proc") && argc >= 3) {
        // nl_main(argv[2]);
    } else {
        printf(usage);
    }

    return 0;
}

