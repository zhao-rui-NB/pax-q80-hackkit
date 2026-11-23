#ifndef Q80_PEDD_PATCH_H
#define Q80_PEDD_PATCH_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Apply the three-word pedd workaround for a device whose secure keys were
 * already cleared. The offsets belong to the 2018 Q80 pedd image; verify them
 * before using a different firmware.
 *
 * The patch keeps the factory pedd handoff, but stops pedd from re-arming the
 * BBL sensor mask and from re-locking the PED switches after iSekVerify reports
 * a CRC error. It does not restore cleared keys.
 */
enum {
    PEDD_BBL_IOCTL_OFFSET = 0x0000d904,
    PEDD_ATTACK_ENTRY_OFFSET = 0x00010160,
    PEDD_ATTACK_RETURN_OFFSET = 0x00010164,
};

static int write_pedd_words(int fd)
{
    static const unsigned int offsets[] = {
        PEDD_BBL_IOCTL_OFFSET,
        PEDD_ATTACK_ENTRY_OFFSET,
        PEDD_ATTACK_RETURN_OFFSET,
    };
    static const unsigned int words[] = {
        0x00006d01U, /* bbl_unlock instead of the sensor mask ioctl */
        0xe3a00000U, /* mov r0, #0 */
        0xe12fff1eU, /* bx lr */
    };

    for (unsigned int index = 0; index < sizeof(words) / sizeof(words[0]);
         ++index) {
        if (pwrite(fd, &words[index], sizeof(words[index]),
                   offsets[index]) != (ssize_t)sizeof(words[index]))
            return -1;
    }
    return 0;
}

static int patch_pedd_in_place(void)
{
    const int fd = open("/usr/bin/pedd", O_RDWR);
    if (fd < 0)
        return -1;
    const int result = write_pedd_words(fd);
    if (close(fd) != 0 && result == 0)
        return -1;
    return result;
}

static int patch_pedd_copy(void)
{
    /*
     * 執行中的 pedd 不能以 O_RDWR 開啟（ETXTBSY）。先複製到同一檔案系統，
     * 修改副本後再 rename；執行中的程序仍使用舊 inode，init 下次啟動時
     * 才會載入新檔。
     */
    static const char source[] = "/usr/bin/pedd";
    static const char temporary[] = "/usr/bin/pedd.q80-new";
    const int source_fd = open(source, O_RDONLY);
    if (source_fd < 0)
        return -1;
    const int destination_fd =
        open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (destination_fd < 0) {
        (void)close(source_fd);
        return -1;
    }

    int result = 0;
    for (;;) {
        const ssize_t copied =
            sendfile(destination_fd, source_fd, NULL, 1 << 20);
        if (copied == 0)
            break;
        if (copied < 0) {
            result = -1;
            break;
        }
    }
    if (result == 0 && write_pedd_words(destination_fd) != 0)
        result = -1;
    if (result == 0 && fchmod(destination_fd, 0755) != 0)
        result = -1;
    (void)close(source_fd);
    if (close(destination_fd) != 0 && result == 0)
        result = -1;
    if (result == 0 && rename(temporary, source) != 0)
        result = -1;
    if (result != 0)
        (void)unlink(temporary);
    return result;
}

static int patch_pedd_handoff(void)
{
    if (patch_pedd_in_place() == 0)
        return 0;
    if (errno != ETXTBSY)
        return -1;
    return patch_pedd_copy();
}

#endif
