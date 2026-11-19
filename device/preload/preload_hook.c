#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * /data/etc/ld.so.preload 會把此 SO 載入每個動態連結的程序。
 * on_load() 只讓 servicemanager 執行一次開機流程：解除 PED 控制、
 * 修改 PID 1 的 ro.fac.tamper_clear，再複製 BusyBox 啟動 runtime 腳本。
 * property 檔案與紀錄格式沿用 patch_tamper.c 的此韌體假設。
 */
typedef unsigned int u32;

enum {
    /* 此版 property 紀錄依序是 name[32]、serial[4]、value。 */
    PROPERTY_NAME_SIZE = 32,
    PROPERTY_VALUE_OFFSET = PROPERTY_NAME_SIZE + 4,
};

static void log_status(const char *status)
{
    /* 這裡可能在 Telnet 啟動前執行，先將階段結果附加到裝置上的檔案。 */
    const int fd = open("/data/tmp/q80_boot_hook.log",
                        O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0)
        return;
    (void)write(fd, status, strlen(status));
    (void)write(fd, "\n", 1);
    (void)close(fd);
}

static int write_value(const char *path, const char *value)
{
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return -1;
    const size_t size = strlen(value);
    const int written = write(fd, value, size) == (ssize_t)size;
    (void)close(fd);
    return written ? 0 : -1;
}

static u32 find_mapping_base(char *maps, u32 size)
{
    /*
     * /proc/1/maps 的每行以「起始位址-結束位址 權限 ... 路徑」開頭。
     * 找到 PID 1 對 property 檔案的可寫共享映射，再解析行首位址。
     * 此韌體從檔案偏移 0 映射，所以映射起點可直接加上紀錄偏移。
     */
    maps[size] = '\0';
    char *line = maps;
    while (*line != '\0') {
        char *next = strchr(line, '\n');
        if (next != NULL)
            *next = '\0';
        if (strstr(line, "/dev/__properties__") != NULL &&
            strstr(line, "rw-s") != NULL) {
            char *after;
            const unsigned long base = strtoul(line, &after, 16);
            if (*after == '-')
                return (u32)base;
        }
        if (next == NULL)
            break;
        line = next + 1;
    }
    return 0;
}

static u32 find_property(const char *area, u32 size)
{
    /* 二進位資料不能當 C 字串；同時比對完整名稱與此韌體的 serial。 */
    static const char name[] = "ro.fac.tamper_clear";
    static const char serial[] = {0, 0, 0, 1};
    for (u32 offset = 0; offset + PROPERTY_VALUE_OFFSET < size; ++offset) {
        if (memcmp(area + offset, name, sizeof(name)) == 0 &&
            memcmp(area + offset + PROPERTY_NAME_SIZE,
                   serial, sizeof(serial)) == 0)
            return offset;
    }
    return 0xffffffffU;
}

static ssize_t read_file(const char *path, char *data, size_t size)
{
    /* read() 可少於要求的長度，讀到 EOF 或填滿緩衝區為止。 */
    const int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    size_t used = 0;
    while (used < size) {
        const ssize_t count = read(fd, data + used, size - used);
        if (count < 0) {
            (void)close(fd);
            return -1;
        }
        if (count == 0)
            break;
        used += (size_t)count;
    }
    (void)close(fd);
    return (ssize_t)used;
}

static int read_property_area(char *area, u32 size)
{
    /* PID 1 的 fd 4 是此韌體的 property backing file；只讀它以找偏移。 */
    return read_file("/proc/1/fd/4", area, size) == size ? 0 : -1;
}

static int patch_init_tamper_clear(void)
{
    char maps[8192 + 1];
    char area[32768];
    int wait_status = 0;
    u32 base;
    u32 property_offset;
    u32 value_address;
    const u32 value_word = 0x00000031U; /* 小端序：'1'、NUL、NUL、NUL。 */
    long result;

    if (read_property_area(area, sizeof(area)) != 0) {
        log_status("HOOK_AREA_OPEN_FAILED");
        return -1;
    }
    property_offset = find_property(area, sizeof(area));
    if (property_offset == 0xffffffffU) {
        log_status("HOOK_PROPERTY_NOT_FOUND");
        return -1;
    }

    const ssize_t maps_used =
        read_file("/proc/1/maps", maps, sizeof(maps) - 1);
    if (maps_used < 0) {
        log_status("HOOK_MAPS_OPEN_FAILED");
        return -1;
    }
    base = find_mapping_base(maps, (u32)maps_used);
    if (base == 0) {
        log_status("HOOK_PROPERTY_MAP_NOT_FOUND");
        return -1;
    }

    /* 停下 PID 1 並等它確實進入停止狀態，才可改寫其記憶體。 */
    result = ptrace(PTRACE_ATTACH, 1, 0, 0);
    if (result < 0) {
        log_status("HOOK_PTRACE_ATTACH_FAILED");
        return -1;
    }
    result = waitpid(1, &wait_status, __WALL);
    if (result != 1 || !WIFSTOPPED(wait_status)) {
        log_status("HOOK_PTRACE_WAIT_FAILED");
        (void)ptrace(PTRACE_DETACH, 1, 0, 0);
        return -1;
    }

    /* 映射起點 + 紀錄偏移 + 36 是 value 在 PID 1 中的虛擬位址。 */
    value_address = base + property_offset + PROPERTY_VALUE_OFFSET;
    result = ptrace(PTRACE_POKEDATA, 1, (void *)(long)value_address,
                    (void *)(long)value_word);
    if (result != 0) {
        log_status("HOOK_PTRACE_WRITE_FAILED");
        (void)ptrace(PTRACE_DETACH, 1, 0, 0);
        return -1;
    }
    (void)ptrace(PTRACE_DETACH, 1, 0, 0);
    if (read_property_area(area, sizeof(area)) != 0 ||
        area[property_offset + PROPERTY_VALUE_OFFSET] != '1') {
        log_status("HOOK_PROPERTY_VERIFY_FAILED");
        return -1;
    }
    return 0;
}

static int copy_file(const char *source, const char *destination, int mode)
{
    /*
     * 開機時 /usr/bin/busybox 可能尚不存在，先從持久保存的 /data 複製。
     * sendfile 在核心內複製資料；循環到 EOF 才算完成，最後設定執行權限。
     */
    const int source_fd = open(source, O_RDONLY);
    if (source_fd < 0)
        return -1;
    const int destination_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC,
                                    mode);
    if (destination_fd < 0) {
        (void)close(source_fd);
        return -1;
    }
    int result = 0;
    for (;;) {
        const ssize_t copied = sendfile(destination_fd, source_fd, NULL, 1 << 20);
        if (copied == 0)
            break;
        if (copied < 0) {
            result = -1;
            break;
        }
    }
    if (fchmod(destination_fd, mode) != 0)
        result = -1;
    (void)close(source_fd);
    (void)close(destination_fd);
    return result;
}

static void spawn_runtime(void)
{
    /* fork 後只在子程序執行腳本，讓 servicemanager 繼續完成啟動。 */
    static const char source[] = "/data/opt/q80_boot/busybox";
    static const char destination[] = "/usr/bin/busybox";
    static char *const argv[] = {
        "busybox", "sh", "/data/opt/q80_boot/q80_boot_runtime.sh", NULL,
    };

    if (copy_file(source, destination, 0755) != 0) {
        log_status("HOOK_BUSYBOX_COPY_FAILED");
        return;
    }
    if (fork() == 0) {
        (void)execve(destination, argv, NULL);
        _exit(1);
    }
}

static void apply_boot_hook(void)
{
    static const char bbl_tmon[] =
        "/sys/devices/platform/iproc-bbl/brcm-iproc-bbl/bbl_tmon_en_dis";
    static const char bbl_unlock[] =
        "/sys/devices/platform/iproc-bbl/brcm-iproc-bbl/bbl_unlock_dis";
    static const char trigger[] = "/sys/pax/ped_trigger_state";

    log_status("HOOK_START");
    /*
     * /proc 供後續讀取 PID 1 的 fd 與 maps。
     * 接著寫入既有的 sysfs 控制檔：bbl_tmon_en_dis=0、
     * bbl_unlock_dis=1、ped_trigger_state=0；沒有刪除任何節點。
     */
    (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)write_value(bbl_tmon, "0");
    (void)write_value(bbl_unlock, "1");
    (void)write_value(trigger, "0");

    const int patched = patch_init_tamper_clear();
    log_status(patched == 0 ? "HOOK_TAMPER_CLEAR_OK" :
                              "HOOK_TAMPER_CLEAR_FAILED");
    spawn_runtime();
    log_status("HOOK_RUNTIME_SPAWNED");
}

__attribute__((constructor))
static void on_load(void)
{
    /* ld.so.preload 會載入每個程序；只有 servicemanager 執行開機操作。 */
    char process_name[17] = {0};
    if (prctl(PR_GET_NAME, (unsigned long)process_name, 0, 0, 0) == 0 &&
        strcmp(process_name, "servicemanager") == 0)
        apply_boot_hook();
}
