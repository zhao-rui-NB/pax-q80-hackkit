typedef unsigned int u32;

static long syscall0(long number)
{
    register long r7 __asm__("r7") = number;
    register long r0 __asm__("r0");

    __asm__ volatile("svc 0" : "=r"(r0) : "r"(r7) : "memory");
    return r0;
}

static long syscall3(long number, long arg0, long arg1, long arg2)
{
    register long r0 __asm__("r0") = arg0;
    register long r1 __asm__("r1") = arg1;
    register long r2 __asm__("r2") = arg2;
    register long r7 __asm__("r7") = number;

    __asm__ volatile("svc 0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r7)
                     : "memory");
    return r0;
}

static long syscall4(long number, long arg0, long arg1, long arg2, long arg3)
{
    register long r0 __asm__("r0") = arg0;
    register long r1 __asm__("r1") = arg1;
    register long r2 __asm__("r2") = arg2;
    register long r3 __asm__("r3") = arg3;
    register long r7 __asm__("r7") = number;

    __asm__ volatile("svc 0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r7)
                     : "memory");
    return r0;
}

static long syscall5(long number, long arg0, long arg1, long arg2,
                     long arg3, long arg4)
{
    register long r0 __asm__("r0") = arg0;
    register long r1 __asm__("r1") = arg1;
    register long r2 __asm__("r2") = arg2;
    register long r3 __asm__("r3") = arg3;
    register long r4 __asm__("r4") = arg4;
    register long r7 __asm__("r7") = number;

    __asm__ volatile("svc 0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r7)
                     : "memory");
    return r0;
}

static u32 text_length(const char *text)
{
    u32 length = 0;
    while (text[length] != '\0')
        ++length;
    return length;
}

static int text_equal(const char *left, const char *right)
{
    u32 index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index])
            return 0;
        ++index;
    }
    return left[index] == right[index];
}

static void write_all(long fd, const char *data, u32 size)
{
    while (size != 0) {
        const long result = syscall3(4, fd, (long)data, size);
        if (result <= 0)
            return;
        data += result;
        size -= (u32)result;
    }
}

static void log_status(const char *status)
{
    static const char path[] = "/data/tmp/q80_boot_hook.log";
    const long fd = syscall3(5, (long)path, 0x441, 0666);
    if (fd < 0)
        return;
    write_all(fd, status, text_length(status));
    write_all(fd, "\n", 1);
    (void)syscall3(6, fd, 0, 0);
}

static void log_hex(const char *label, u32 value)
{
    static const char digits[] = "0123456789abcdef";
    static const char path[] = "/data/tmp/q80_boot_hook.log";
    char text[64];
    u32 offset = 0;
    const long fd = syscall3(5, (long)path, 0x441, 0666);

    if (fd < 0)
        return;
    while (label[offset] != '\0' && offset < 48) {
        text[offset] = label[offset];
        ++offset;
    }
    for (u32 shift = 28; shift <= 28; shift -= 4) {
        text[offset++] = digits[(value >> shift) & 0xfU];
        if (shift == 0)
            break;
    }
    text[offset++] = '\n';
    write_all(fd, text, offset);
    (void)syscall3(6, fd, 0, 0);
}

static void get_process_name(char *name, u32 size)
{
    for (u32 index = 0; index < size; ++index)
        name[index] = '\0';
    (void)syscall3(172, 16, (long)name, 0);
}

static void ensure_proc_mounted(void)
{
    static const char source[] = "proc";
    static const char target[] = "/proc";
    static const char filesystem[] = "proc";

    (void)syscall5(
        21, (long)source, (long)target, (long)filesystem, 0, 0);
}

static int write_value(const char *path, const char *value)
{
    const long fd = syscall3(5, (long)path, 0x241, 0666);
    if (fd < 0)
        return -1;
    write_all(fd, value, text_length(value));
    (void)syscall3(6, fd, 0, 0);
    return 0;
}

static int contains(const char *text, u32 size, const char *needle)
{
    const u32 needle_length = text_length(needle);
    for (u32 index = 0; index + needle_length <= size; ++index) {
        u32 matched = 0;
        while (matched < needle_length &&
               text[index + matched] == needle[matched])
            ++matched;
        if (matched == needle_length)
            return 1;
    }
    return 0;
}

static u32 find_mapping_base(const char *maps, u32 size)
{
    static const char marker[] = "/dev/__properties__";
    static const char writable[] = "rw-s";

    for (u32 index = 0; index < size; ++index) {
        if (index != 0 && maps[index - 1] != '\n')
            continue;

        u32 line_end = index;
        while (line_end < size && maps[line_end] != '\n')
            ++line_end;
        if (!contains(maps + index, line_end - index, marker) ||
            !contains(maps + index, line_end - index, writable))
            continue;

        u32 value = 0;
        u32 cursor = index;
        while (cursor < size && maps[cursor] != '\n') {
            const char ch = maps[cursor++];
            u32 digit;
            if (ch >= '0' && ch <= '9')
                digit = (u32)(ch - '0');
            else if (ch >= 'a' && ch <= 'f')
                digit = (u32)(ch - 'a' + 10);
            else if (ch == '-')
                return value;
            else
                break;
            value = (value << 4) | digit;
        }
    }
    return 0;
}

static u32 find_property(const char *area, u32 size)
{
    static const char name[] = "ro.fac.tamper_clear";
    const u32 name_length = sizeof(name) - 1;

    for (u32 offset = 0; offset + 36 < size; ++offset) {
        u32 matched = 0;
        while (matched < name_length &&
               area[offset + matched] == name[matched])
            ++matched;
        if (matched != name_length)
            continue;
        if (area[offset + name_length] != '\0')
            continue;
        if (area[offset + 32] == 0 && area[offset + 33] == 0 &&
            area[offset + 34] == 0 && area[offset + 35] == 1)
            return offset;
    }
    return 0xffffffffU;
}

static int read_property_area(char *area, u32 size)
{
    static const char path[] = "/proc/1/fd/4";
    const long fd = syscall3(5, (long)path, 0, 0);
    u32 used = 0;

    if (fd < 0)
        return -1;
    while (used < size) {
        const long result =
            syscall3(3, fd, (long)(area + used), size - used);
        if (result <= 0)
            break;
        used += (u32)result;
    }
    (void)syscall3(6, fd, 0, 0);
    return used == size ? 0 : -1;
}

static int patch_init_tamper_clear(void)
{
    static const char maps_path[] = "/proc/1/maps";
    char maps[8192];
    char area[32768];
    u32 maps_used = 0;
    int wait_status = 0;
    u32 base;
    u32 property_offset;
    u32 value_address;
    const u32 value_word = 0x00000031U;
    long maps_fd;
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

    maps_fd = syscall3(5, (long)maps_path, 0, 0);
    if (maps_fd < 0) {
        log_status("HOOK_MAPS_OPEN_FAILED");
        return -1;
    }
    while (maps_used < sizeof(maps)) {
        result = syscall3(
            3, maps_fd, (long)(maps + maps_used), sizeof(maps) - maps_used);
        if (result <= 0)
            break;
        maps_used += (u32)result;
    }
    (void)syscall3(6, maps_fd, 0, 0);

    base = find_mapping_base(maps, maps_used);
    if (base == 0) {
        log_status("HOOK_PROPERTY_MAP_NOT_FOUND");
        return -1;
    }

    result = syscall4(26, 16, 1, 0, 0);
    (void)syscall4(114, 1, (long)&wait_status, 0, 0);
    if (result < 0) {
        log_status("HOOK_PTRACE_ATTACH_FAILED");
        return -1;
    }

    value_address = base + property_offset + 36;
    result = syscall4(26, 5, 1, value_address, value_word);
    if (result != 0) {
        log_status("HOOK_PTRACE_WRITE_FAILED");
        (void)syscall4(26, 17, 1, 0, 0);
        return -1;
    }

    (void)syscall4(26, 17, 1, 0, 0);
    if (read_property_area(area, sizeof(area)) != 0 ||
        area[property_offset + 36] != '1') {
        log_status("HOOK_PROPERTY_VERIFY_FAILED");
        return -1;
    }
    return 0;
}

static int copy_file(const char *source, const char *destination, int mode)
{
    char buffer[4096];
    long source_fd;
    long destination_fd;

    source_fd = syscall3(5, (long)source, 0, 0);
    if (source_fd < 0)
        return -1;
    destination_fd = syscall3(5, (long)destination, 0x241, mode);
    if (destination_fd < 0) {
        (void)syscall3(6, source_fd, 0, 0);
        return -1;
    }

    for (;;) {
        const long count =
            syscall3(3, source_fd, (long)buffer, sizeof(buffer));
        if (count < 0) {
            (void)syscall3(6, source_fd, 0, 0);
            (void)syscall3(6, destination_fd, 0, 0);
            return -1;
        }
        if (count == 0)
            break;
        write_all(destination_fd, buffer, (u32)count);
    }

    (void)syscall3(6, source_fd, 0, 0);
    (void)syscall3(6, destination_fd, 0, 0);
    (void)syscall3(15, (long)destination, mode, 0);
    return 0;
}

static void spawn_runtime(void)
{
    static const char busybox_source[] =
        "/data/opt/q80_boot/busybox";
    static const char busybox_runtime[] =
        "/usr/bin/busybox";
    static const char script[] =
        "/data/opt/q80_boot/q80_boot_runtime.sh";
    static char *const argv[] = {
        "busybox",
        "sh",
        (char *)script,
        0,
    };

    if (copy_file(busybox_source, busybox_runtime, 0755) != 0) {
        log_status("HOOK_BUSYBOX_COPY_FAILED");
        return;
    }

    if (syscall0(2) == 0) {
        (void)syscall3(11, (long)busybox_runtime, (long)argv, 0);
        (void)syscall0(1);
    }
}

static void apply_boot_hook(void)
{
    static const char bbl_tmon[] =
        "/sys/devices/platform/iproc-bbl/brcm-iproc-bbl/bbl_tmon_en_dis";
    static const char bbl_unlock[] =
        "/sys/devices/platform/iproc-bbl/brcm-iproc-bbl/bbl_unlock_dis";
    static const char trigger[] =
        "/sys/pax/ped_trigger_state";
    int patched;

    log_status("HOOK_START");
    ensure_proc_mounted();
    (void)write_value(bbl_tmon, "0");
    (void)write_value(bbl_unlock, "1");
    (void)write_value(trigger, "0");

    patched = patch_init_tamper_clear();
    log_status(patched == 0 ? "HOOK_TAMPER_CLEAR_OK" :
                              "HOOK_TAMPER_CLEAR_FAILED");
    spawn_runtime();
    log_status("HOOK_RUNTIME_SPAWNED");
}

#ifdef Q80_STANDALONE
__attribute__((used, noinline))
static void standalone_entry(void)
{
    apply_boot_hook();
    (void)syscall0(1);
}

__attribute__((naked, noreturn))
void _start(void)
{
    __asm__ volatile(
        "bl standalone_entry\n"
        "mov r0, #0\n"
        "mov r7, #1\n"
        "svc 0\n"
    );
}
#else
__attribute__((constructor))
static void on_load(void)
{
    char process_name[17];

    get_process_name(process_name, sizeof(process_name));
    if (text_equal(process_name, "servicemanager"))
        apply_boot_hook();
}
#endif
