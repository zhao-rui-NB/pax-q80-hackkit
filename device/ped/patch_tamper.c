#define _GNU_SOURCE
#include <fcntl.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * 只處理本次開機的 ro.fac.tamper_clear。
 * PID 1 持有 Android property area，這個 ro.* 值不能透過一般 property setter 更新。
 * 流程：讀取 PID 1 的 property 檔案找紀錄偏移 → 查 PID 1 的映射起點 →
 *       暫停 PID 1 並修改其記憶體 → 重新讀取 property 檔案確認結果。
 * /proc/1/fd/4、檔案大小和紀錄格式都是此韌體的假設；換韌體需要重新確認。
 */
typedef unsigned int u32;

enum {
    /* 此版 prop_info 依序存放 name[32]、serial[4]、value。 */
    PROPERTY_NAME_SIZE = 32,
    PROPERTY_VALUE_OFFSET = PROPERTY_NAME_SIZE + 4,
};

static u32 find_mapping_base(const char *maps, u32 size)
{
    /*
     * maps 每行先列出「起始位址-結束位址 權限 ... 路徑」。
     * 找 /dev/__properties__ 的 rw-s（可寫、共享）映射，取行首十六進位位址。
     * 此韌體從檔案偏移 0 開始映射，因此「映射起點 + 紀錄偏移」就是
     * 同一筆紀錄在 PID 1 記憶體中的位置。
     */
    static const char marker[] = "/dev/__properties__";
    static const char writable[] = "rw-s";
    for (u32 i = 0; i < size; ++i) {
        if (i != 0 && maps[i - 1] != '\n')
            continue;
        u32 line_end = i;
        while (line_end < size && maps[line_end] != '\n')
            ++line_end;
        /* maps 未以 NUL 結尾；memmem 會把搜尋限制在這一行。 */
        if (!memmem(maps + i, line_end - i, marker, sizeof(marker) - 1) ||
            !memmem(maps + i, line_end - i, writable, sizeof(writable) - 1))
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
    /*
     * property area 是二進位資料，不能把它當字串搜尋。
     * 找到完整的屬性名稱後，再檢查名稱後的 NUL 和 serial，降低誤判。
     * 回傳的是紀錄在檔案中的起始偏移，不是 value 的偏移。
     */
    static const char name[] = "ro.fac.tamper_clear";
    const u32 name_length = sizeof(name) - 1;

    for (u32 offset = 0; offset + PROPERTY_VALUE_OFFSET < size; ++offset) {
        u32 matched = 0;
        while (matched < name_length &&
               area[offset + matched] == name[matched])
            ++matched;
        if (matched != name_length)
            continue;
        if (area[offset + name_length] != '\0')
            continue;
        /* 此韌體這筆一字元屬性的 serial 位元組是 00 00 00 01。 */
        if (area[offset + PROPERTY_NAME_SIZE] == 0 &&
            area[offset + PROPERTY_NAME_SIZE + 1] == 0 &&
            area[offset + PROPERTY_NAME_SIZE + 2] == 0 &&
            area[offset + PROPERTY_NAME_SIZE + 3] == 1)
            return offset;
    }
    return 0xffffffffU;
}

static int read_property_area(char *area, u32 size)
{
    /*
     * /proc/1/fd/4 是 PID 1 開啟的 property area 檔案。
     * 這裡只讀取它以取得紀錄偏移；實際修改發生在 PID 1 的記憶體。
     * 必須讀滿指定大小，否則後續搜尋可能用到不完整的資料。
     */
    static const char area_path[] = "/proc/1/fd/4";
    long fd;
    u32 used = 0;

    fd = open(area_path, O_RDONLY);
    if (fd < 0)
        return -1;
    while (used < size) {
        long result = read(fd, area + used, size - used);
        if (result <= 0)
            break;
        used += (u32)result;
    }
    (void)close(fd);
    return used == size ? 0 : -1;
}

int patch_tamper_clear(void)
{
    /*
     * bypass.sh 檢查成功標記；失敗時也回傳非零狀態碼。
     * 狀態訊息都很短，直接 write 一次；若成功標記只寫出一部分，
     * 腳本的 grep 也不會將它誤認為成功。
     */
    static const char maps_path[] = "/proc/1/maps";
    static const char ok[] = "TAMPER_CLEAR_PTRACE_OK\n";
    static const char maps_error[] = "TAMPER_CLEAR_PTRACE_MAPS_ERROR\n";
    static const char area_error[] = "TAMPER_CLEAR_PTRACE_AREA_ERROR\n";
    static const char record_error[] = "TAMPER_CLEAR_PTRACE_RECORD_ERROR\n";
    static const char attach_error[] = "TAMPER_CLEAR_PTRACE_ATTACH_ERROR\n";
    static const char wait_error[] = "TAMPER_CLEAR_PTRACE_WAIT_ERROR\n";
    static const char post_verify_error[] = "TAMPER_CLEAR_PTRACE_POST_VERIFY_ERROR\n";
    static const char write_error[] = "TAMPER_CLEAR_PTRACE_WRITE_ERROR\n";
    char maps[8192];
    char area[32768];
    long maps_fd;
    long result;
    int wait_status = 0;
    u32 used = 0;
    u32 base;
    u32 property_offset;
    u32 value_address;
    u32 value_word = 0x00000031U; /* 小端序：'1'、NUL、NUL、NUL。 */

    /* 第一階段：從 property 檔案找屬性紀錄，得到它在檔案內的偏移。 */
    if (read_property_area(area, sizeof(area)) != 0) {
        (void)write(STDOUT_FILENO, area_error, sizeof(area_error) - 1);
        return 1;
    }
    property_offset = find_property(area, sizeof(area));
    if (property_offset == 0xffffffffU) {
        (void)write(STDOUT_FILENO, record_error, sizeof(record_error) - 1);
        return 1;
    }

    /* 第二階段：從 maps 找到同一個檔案在 PID 1 的記憶體映射起點。 */
    maps_fd = open(maps_path, O_RDONLY);
    if (maps_fd < 0) {
        (void)write(STDOUT_FILENO, maps_error, sizeof(maps_error) - 1);
        return 1;
    }
    while (used < sizeof(maps)) {
        result = read(maps_fd, maps + used, sizeof(maps) - used);
        if (result <= 0)
            break;
        used += (u32)result;
    }
    (void)close(maps_fd);
    base = find_mapping_base(maps, used);
    if (base == 0) {
        (void)write(STDOUT_FILENO, maps_error, sizeof(maps_error) - 1);
        return 1;
    }

    /*
     * 第三階段：PTRACE_ATTACH 要求 PID 1 停止，waitpid 確認它真的停下。
     * __WALL 允許等待被追蹤的工作；若尚未停止就寫記憶體，結果不可靠。
     */
    result = ptrace(PTRACE_ATTACH, 1, 0, 0);
    if (result < 0) {
        (void)write(STDOUT_FILENO, attach_error, sizeof(attach_error) - 1);
        return 1;
    }
    result = waitpid(1, &wait_status, __WALL);
    if (result != 1 || !WIFSTOPPED(wait_status)) {
        (void)write(STDOUT_FILENO, wait_error, sizeof(wait_error) - 1);
        (void)ptrace(PTRACE_DETACH, 1, 0, 0);
        return 1;
    }

    /*
     * 紀錄偏移 + 36 指向 value；再加映射起點得到 PID 1 的虛擬位址。
     * PTRACE_POKEDATA 一次寫入一個 ARM word，也就是上面的四個位元組。
     * 不論寫入成功與否，都要 DETACH，讓 PID 1 繼續執行。
     */
    value_address = base + property_offset + PROPERTY_VALUE_OFFSET;
    result = ptrace(PTRACE_POKEDATA, 1, (void *)(long)value_address,
                    (void *)(long)value_word);
    if (result != 0) {
        (void)write(STDOUT_FILENO, write_error, sizeof(write_error) - 1);
        (void)ptrace(PTRACE_DETACH, 1, 0, 0);
        return 1;
    }
    (void)ptrace(PTRACE_DETACH, 1, 0, 0);

    /* 第四階段：重新讀取 backing file，確認同一筆屬性的 value 已變成 '1'。 */
    if (read_property_area(area, sizeof(area)) != 0 ||
        find_property(area, sizeof(area)) == 0xffffffffU ||
        area[property_offset + PROPERTY_VALUE_OFFSET] != '1') {
        (void)write(STDOUT_FILENO, post_verify_error, sizeof(post_verify_error) - 1);
        return 1;
    }
    (void)write(STDOUT_FILENO, ok, sizeof(ok) - 1);
    return 0;
}

__attribute__((noreturn))
void _start(void)
{
    /*
     * -nostdlib 省略了通常呼叫 main() 的啟動程式。
     * 動態載入器會進入 _start；這裡用 C 呼叫主流程，並傳出其結束狀態。
     */
    _exit(patch_tamper_clear());
}
