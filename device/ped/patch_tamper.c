typedef unsigned int u32;

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

static void write_all(const char *data, u32 size)
{
    while (size != 0) {
        const long result = syscall3(4, 1, (long)data, size);
        if (result <= 0)
            return;
        data += result;
        size -= (u32)result;
    }
}

static int contains(const char *text, u32 size, const char *needle)
{
    u32 needle_length = 0;
    while (needle[needle_length] != '\0')
        ++needle_length;
    for (u32 i = 0; i + needle_length <= size; ++i) {
        u32 j = 0;
        while (j < needle_length && text[i + j] == needle[j])
            ++j;
        if (j == needle_length)
            return 1;
    }
    return 0;
}

static u32 find_mapping_base(const char *maps, u32 size)
{
    static const char marker[] = "/dev/__properties__";
    static const char writable[] = "rw-s";
    for (u32 i = 0; i < size; ++i) {
        if (i != 0 && maps[i - 1] != '\n')
            continue;
        u32 line_end = i;
        while (line_end < size && maps[line_end] != '\n')
            ++line_end;
        if (!contains(maps + i, line_end - i, marker) ||
            !contains(maps + i, line_end - i, writable))
            continue;
        u32 value = 0;
        u32 cursor = i;
        while (cursor < size && maps[cursor] != '\n') {
            char ch = maps[cursor++];
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
    static const char area_path[] = "/proc/1/fd/4";
    long fd;
    u32 used = 0;

    fd = syscall3(5, (long)area_path, 0, 0);
    if (fd < 0)
        return -1;
    while (used < size) {
        long result = syscall3(3, fd, (long)(area + used), size - used);
        if (result <= 0)
            break;
        used += (u32)result;
    }
    (void)syscall3(6, fd, 0, 0);
    return used == size ? 0 : -1;
}

void patch_tamper_clear(void)
{
    static const char maps_path[] = "/proc/1/maps";
    static const char ok[] = "TAMPER_CLEAR_PTRACE_OK\n";
    static const char maps_error[] = "TAMPER_CLEAR_PTRACE_MAPS_ERROR\n";
    static const char area_error[] = "TAMPER_CLEAR_PTRACE_AREA_ERROR\n";
    static const char record_error[] = "TAMPER_CLEAR_PTRACE_RECORD_ERROR\n";
    static const char attach_error[] = "TAMPER_CLEAR_PTRACE_ATTACH_ERROR\n";
    static const char post_verify_error[] = "TAMPER_CLEAR_PTRACE_POST_VERIFY_ERROR\n";
    static const char write_error[] = "TAMPER_CLEAR_PTRACE_WRITE_ERROR\n";
    char maps[8192];
    char area[32768];
    long maps_fd;
    long result;
    u32 used = 0;
    u32 base;
    u32 property_offset;
    u32 value_address;
    u32 value_word = 0x00000031U;

    if (read_property_area(area, sizeof(area)) != 0) {
        write_all(area_error, sizeof(area_error) - 1);
        return;
    }
    property_offset = find_property(area, sizeof(area));
    if (property_offset == 0xffffffffU) {
        write_all(record_error, sizeof(record_error) - 1);
        return;
    }

    maps_fd = syscall3(5, (long)maps_path, 0, 0);
    if (maps_fd < 0) {
        write_all(maps_error, sizeof(maps_error) - 1);
        return;
    }
    while (used < sizeof(maps)) {
        result = syscall3(3, maps_fd, (long)(maps + used), sizeof(maps) - used);
        if (result <= 0)
            break;
        used += (u32)result;
    }
    (void)syscall3(6, maps_fd, 0, 0);
    base = find_mapping_base(maps, used);
    if (base == 0) {
        write_all(maps_error, sizeof(maps_error) - 1);
        return;
    }

    result = syscall4(26, 16, 1, 0, 0);
    (void)syscall3(7, 1, (long)&result, 0x40000000U);
    if (result < 0) {
        write_all(attach_error, sizeof(attach_error) - 1);
        return;
    }

    value_address = base + property_offset + 36;
    result = syscall4(26, 5, 1, value_address, value_word);
    if (result != 0) {
        write_all(write_error, sizeof(write_error) - 1);
        (void)syscall4(26, 17, 1, 0, 0);
        return;
    }
    (void)syscall4(26, 17, 1, 0, 0);

    if (read_property_area(area, sizeof(area)) != 0 ||
        find_property(area, sizeof(area)) == 0xffffffffU ||
        area[property_offset + 36] != '1') {
        write_all(post_verify_error, sizeof(post_verify_error) - 1);
        return;
    }
    write_all(ok, sizeof(ok) - 1);
}

__attribute__((naked, noreturn))
void _start(void)
{
    __asm__ volatile(
        "bl patch_tamper_clear\n"
        "mov r0, #0\n"
        "mov r7, #1\n"
        "svc 0\n"
    );
}
