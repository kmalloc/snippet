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
            perror("poll");
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

    for (int i = 0; i < argc; i++) {
        ret.wd[i] = inotify_add_watch(ret.fd, argv[i], IN_OPEN | IN_CLOSE | IN_CREATE | IN_DELETE | IN_MODIFY);
        if (ret.wd[i] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", argv[i]);
            perror("inotify_add_watch");
            continue;
        }
    }

    return ret;
}

static int enter_proc_ns(const char* pid, int type)  {
    int ori_fd = -1;
    char proc_ns_path[256] = {0};
    if (type == CLONE_NEWNS) {
        ori_fd = open("/proc/self/ns/mnt", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/mnt", pid);
    } else if (type == CLONE_NEWIPC) {
        ori_fd = open("/proc/self/ns/ipc", O_RDONLY);
        snprintf(proc_ns_path, sizeof(proc_ns_path), "/proc/%s/ns/ipc", pid);
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
    int ret = syscall(308, ns_fd, type);
    if (ret < 0) {
        printf("setns failed\n");
        close(ori_fd);
        return -1;
    }

    close(ns_fd);
    return ori_fd;
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

    if (back) { 
        printf("now return to original namespace, exec cmd:pwd;whoami;cd ~;pwd;ls\n");

        ret = syscall(308, ori_fd, CLONE_NEWNS);
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

static void* create_shm(size_t key, bool create) {
    int id = shmget(key, 32, 0666);    
    if (id == -1 && !create) return 0;

    if (id == -1) {
        id = shmget(key, 32, 0666|IPC_CREAT|IPC_EXCL);
        if (id == -1) return 0;
    }

    return shmat(id, NULL, 0);
}

static void ipc_op(bool create, const char* pid) {
    const size_t key = 0x23342422;
    if (pid) {
        int ori_fd = enter_proc_ns(pid, CLONE_NEWIPC);
        if (ori_fd < 0) return;

        printf("now to get shm in container\n");
        int* p = (int*)create_shm(key, create);

        if (!p) {
            printf("get shm fails in namespace\n");
        } else {
            printf("get shm succ, value:0x%x, should be:0x%x\n", *p, key);
        }

        close(ori_fd);
    } else if (create) {
        int* p = (int*)create_shm(key, true);
        *p = key;
        printf("create shm done, value:%x\n", key);
    }
}


static void cgroup_op(const char* cg_file) {
    if (cg_file) {
        int fd = open(cg_file, 'a');
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

    while (1) {
    }
}

// docker inspect --format {{.State.Pid}} $container_id
// get nmt namespace id from process: /proc/$pid/ns/mnt.
//
int main(int argc, char* argv[]) {
    const static char* usage = "Usage: (cgroup cg_path), (ipc create(or no) pid), (mnt pid back(or null))\n";
    if (argc <= 1) {
        printf(usage);
    } else if (!strcmp(argv[1], "ipc") && argc >= 3) {
        ipc_op(!strcmp(argv[2], "create"), argc == 4?argv[3]:NULL);
    } else if (!strcmp(argv[1], "mnt") && argc >= 3) {
        mnt_op(argv[2], argc == 4 && !strcmp(argv[3], "back"));
    } else if (!strcmp(argv[1], "cgroup")) {
        const char* cg = argc >= 3?argv[2]:NULL;
        cgroup_op(cg);
    } else {
        printf(usage);
    }

    return 0;
}

