/* Interactive Q80 thermal printer tests. Nothing prints on app startup. */
#include <osal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xui.h>

#define WIDTH 480
#define HEIGHT 800
#define ACTION_KEY 1000
#define EXIT_KEY 1100
#define CHINESE_WIDTH 384
#define CHINESE_HEIGHT 220
#define IMAGE_WIDTH 384
#define IMAGE_HEIGHT 240
#define IMAGE_PACKET_CAPACITY (1 + IMAGE_HEIGHT * (2 + IMAGE_WIDTH / 8))

enum Action { CHECK, TEXT, SIZES, CHINESE, IMAGE, FEED, DENSITY, FONTS, ACTION_COUNT };

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static XuiWindow *screen;
static XuiFont *font;
static Button actions[ACTION_COUNT];
static Button exit_button;
static unsigned char image_packet[IMAGE_PACKET_CAPACITY];
static const char *const labels[ACTION_COUNT] = {
    "CHECK", "TEXT", "SIZES", "CHINESE", "IMAGE", "FEED", "DENSITY", "FONTS"
};

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void show_status(const char *line1, const char *line2)
{
    XuiCanvasDrawRect(screen, 20, 565, 440, 122, rgb(47, 55, 64), 1, 1);
    XuiCanvasDrawText(screen, 34, 577, 29, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)line1);
    XuiCanvasDrawText(screen, 34, 627, 26, font, XUI_BOLD,
                      rgb(207, 221, 226), (char *)line2);
}

static int make_button(Button *button, int x, int y, int width, int height,
                       const char *label, int key, int background,
                       int foreground)
{
    button->window = XuiCreateButton(screen, x, y, width, height);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = foreground;
    button->normal.text_font = font;
    button->normal.text_x = 15;
    button->normal.text_y = 20;
    button->normal.text_height = 32;
    button->pressed = button->normal;
    button->pressed.btn_bg = rgb(159, 205, 175);
    if (XuiButtonSetStat(button->window, XUI_BTN_NORMAL, &button->normal) < 0 ||
        XuiButtonSetStat(button->window, XUI_BTN_PRESSED, &button->pressed) < 0 ||
        XuiButtonSetKey(button->window, key) < 0)
        return -1;
    XuiShowWindow(button->window, 1, 0);
    return 0;
}

static void check_printer(void)
{
    int opened = OsPrnOpen(PRN_REAL, NULL);
    int status, lines;
    char first[80], second[80];
    if (opened != RET_OK) {
        snprintf(first, sizeof(first), "OPEN FAILED: %d", opened);
        show_status(first, "Check paper and power");
        printf("Printer open=%d\n", opened);
        fflush(stdout);
        return;
    }
    status = OsPrnCheck();
    lines = OsPrnGetDotLine();
    OsPrnClose();
    snprintf(first, sizeof(first), "OPEN %d  CHECK %d", opened, status);
    snprintf(second, sizeof(second), "DOT LINES %d", lines);
    show_status(first, second);
    printf("Printer check=%d dot_lines=%d\n", status, lines);
    fflush(stdout);
}

static void list_fonts(void)
{
    FT_FONT *list = NULL;
    int count = OsEnumFont(&list);
    int i;
    char first[80], second[80];
    snprintf(first, sizeof(first), "FONT COUNT %d", count);
    if (count > 0 && list) {
        snprintf(second, sizeof(second), "FIRST: %.48s", list[0].FileName);
        for (i = 0; i < count && i < 32; ++i) {
            printf("Font %d: %.64s (%.64s)\n", i, list[i].FileName,
                   list[i].FontName);
        }
        fflush(stdout);
    } else {
        snprintf(second, sizeof(second), "NO FONTS FOUND");
    }
    show_status(first, second);
}

static int queue_canvas_image(XuiWindow *work, int width, int height,
                              int threshold)
{
    unsigned char *rgba;
    XuiImg *image;
    int row, col, row_bytes, index = 0, value, dark_pixels = 0;
    if (!work || width % 8 || height > 255 ||
        1 + height * (2 + width / 8) > IMAGE_PACKET_CAPACITY)
        return -1;
    row_bytes = width / 8;
    image = XuiCaptureCanvas(work, 0, 0, width, height);
    XuiDestroyWindow(work);
    if (!image)
        return -1;
    rgba = malloc(width * height * 4);
    if (!rgba) {
        XuiImgFree(image);
        return -1;
    }
    value = XuiImgToRgba(image, (char *)rgba);
    XuiImgFree(image);
    if (value < 0) {
        free(rgba);
        return -1;
    }
    image_packet[index++] = (unsigned char)height;
    for (row = 0; row < height; ++row) {
        image_packet[index++] = (unsigned char)(row_bytes >> 8);
        image_packet[index++] = (unsigned char)row_bytes;
        for (col = 0; col < row_bytes; ++col) {
            unsigned char bits = 0;
            int bit;
            for (bit = 0; bit < 8; ++bit) {
                int x = col * 8 + bit;
                int pixel = (row * width + x) * 4;
                int brightness = rgba[pixel] + rgba[pixel + 1] +
                                 rgba[pixel + 2];
                if (brightness < threshold) {
                    bits |= (unsigned char)(0x80 >> bit);
                    ++dark_pixels;
                }
            }
            image_packet[index++] = bits;
        }
    }
    free(rgba);
    if (dark_pixels < 30)
        return -1;
    OsPrnPutImage(image_packet);
    return 0;
}

static int add_chinese_image(void)
{
    XuiWindow *work = XuiCreateCanvas(screen, 0, 0,
                                      CHINESE_WIDTH, CHINESE_HEIGHT);
    if (!work)
        return -1;
    XuiCanvasSetBackground(work, XUI_BG_NORMAL, 0, rgb(255, 255, 255));
    XuiCanvasDrawText(work, 100, 4, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "中");
    XuiCanvasDrawText(work, 220, 4, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "文");
    XuiCanvasDrawText(work, 24, 78, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "打");
    XuiCanvasDrawText(work, 146, 78, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "印");
    XuiCanvasDrawText(work, 268, 78, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "機");
    XuiCanvasDrawText(work, 100, 152, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "測");
    XuiCanvasDrawText(work, 220, 152, 64, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "試");
    return queue_canvas_image(work, CHINESE_WIDTH, CHINESE_HEIGHT, 300);
}

static int add_test_image(void)
{
    XuiWindow *work = XuiCreateCanvas(screen, 0, 0,
                                      IMAGE_WIDTH, IMAGE_HEIGHT);
    int row, col;
    if (!work)
        return -1;
    XuiCanvasSetBackground(work, XUI_BG_NORMAL, 0, rgb(255, 255, 255));
    XuiCanvasDrawText(work, 18, 12, 72, font, XUI_BOLD,
                      rgb(0, 0, 0), "Q80");
    XuiCanvasDrawText(work, 18, 98, 34, font, XUI_BOLD,
                      rgb(0, 0, 0), "IMAGE TEST");
    for (row = 0; row < 4; ++row)
        for (col = 0; col < 16; ++col)
            if ((row + col) % 2 == 0)
                XuiCanvasDrawRect(work, 16 + col * 22, 148 + row * 22,
                                  20, 20, rgb(0, 0, 0), 0, 1);
    return queue_canvas_image(work, IMAGE_WIDTH, IMAGE_HEIGHT, 384);
}

static int add_raster_text_comparison(void)
{
    XuiWindow *work = XuiCreateCanvas(screen, 0, 0, 384, 80);
    if (!work)
        return -1;
    XuiCanvasSetBackground(work, XUI_BG_NORMAL, 0, rgb(255, 255, 255));
    XuiCanvasDrawText(work, 16, 3, 48, font, XUI_TEXT_NORMAL,
                      rgb(0, 0, 0), "SIZE 24x48");
    XuiCanvasDrawRect(work, 16, 65, 352, 1, rgb(0, 0, 0), 0, 1);
    XuiCanvasDrawRect(work, 16, 71, 352, 2, rgb(0, 0, 0), 0, 1);
    return queue_canvas_image(work, 384, 80, 384);
}

static void print_test(enum Action action)
{
    int opened, check, result;
    char first[80], second[80];
    opened = OsPrnOpen(PRN_REAL, NULL);
    if (opened != RET_OK) {
        snprintf(first, sizeof(first), "OPEN FAILED: %d", opened);
        show_status(first, "No paper was printed");
        printf("Printer open=%d\n", opened);
        fflush(stdout);
        return;
    }
    check = OsPrnCheck();
    if (check != RET_OK) {
        OsPrnClose();
        snprintf(first, sizeof(first), "PRINTER CHECK: %d", check);
        show_status(first, "No paper was printed");
        printf("Printer check=%d\n", check);
        fflush(stdout);
        return;
    }
    OsPrnClrBuf();
    OsPrnSetGray(4);
    OsPrnSetIndent(0, 0);
    OsPrnSetSpace(0, 0);
    OsPrnSelectFontSize(12, 24, 24, 24);
    if (action == TEXT) {
        OsPrnPrintf("Q80 PRINTER TEST\n");
        OsPrnPrintf("ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");
        OsPrnPrintf("0123456789 !@#$%%&*()\n");
        OsPrnPrintf("------------------------\n");
    } else if (action == SIZES) {
        OsPrnPrintf("FONT SIZE TEST 12x24\n");
        OsPrnSelectFontSize(16, 32, 32, 32);
        OsPrnPrintf("SIZE 16x32\n");
        OsPrnSelectFontSize(24, 48, 48, 48);
        OsPrnPrintf("SIZE 24x48\n");
        if (add_raster_text_comparison() < 0) {
            OsPrnClose();
            show_status("COMPARE PREP FAILED", "No paper was printed");
            printf("Printer raster comparison preparation failed\n");
            fflush(stdout);
            return;
        }
    } else if (action == CHINESE) {
        if (add_chinese_image() < 0) {
            OsPrnClose();
            show_status("CHINESE IMAGE FAILED", "No paper was printed");
            printf("Printer Chinese image preparation failed\n");
            fflush(stdout);
            return;
        }
    } else if (action == IMAGE) {
        OsPrnPrintf("IMAGE TEST\n");
        if (add_test_image() < 0) {
            OsPrnClose();
            show_status("IMAGE PREP FAILED", "No paper was printed");
            printf("Printer image preparation failed\n");
            fflush(stdout);
            return;
        }
    } else if (action == FEED) {
        OsPrnPrintf("FEED TEST\n");
    } else if (action == DENSITY) {
        OsPrnPrintf("DENSITY LEVEL 4\n");
        OsPrnPrintf("########################\n");
    }
    OsPrnFeed(action == FEED ? 96 : 48);
    result = OsPrnStart();
    OsPrnClose();
    snprintf(first, sizeof(first), "%s RESULT: %d", labels[action], result);
    snprintf(second, sizeof(second), "CHECK %d  (0 = OK)", check);
    show_status(first, second);
    printf("Printer %s result=%d\n", labels[action], result);
    fflush(stdout);
}

int app_main(int argc, char **argv)
{
    int i, key, result = 1;
    (void)argc;
    (void)argv;
    if (XuiOpen(argc, argv) < 0)
        return 1;
    screen = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!screen || !font)
        goto cleanup;
    XuiCanvasSetBackground(screen, XUI_BG_NORMAL, 0, rgb(8, 11, 15));
    XuiCanvasDrawRect(screen, 20, 20, 8, 75, rgb(0, 201, 52), 0, 1);
    XuiCanvasDrawText(screen, 43, 15, 48, font, XUI_BOLD,
                      rgb(255, 255, 255), "Q80 PRINTER");
    XuiCanvasDrawText(screen, 43, 76, 26, font, XUI_BOLD,
                      rgb(184, 201, 208), "TAP ONE TEST AT A TIME");
    for (i = 0; i < ACTION_COUNT; ++i) {
        if (make_button(&actions[i], 20 + (i % 2) * 230,
                        170 + (i / 2) * 94, 210, 76, labels[i],
                        ACTION_KEY + i,
                        i == TEXT ? rgb(0, 201, 52) : rgb(255, 255, 255),
                        rgb(16, 16, 16)) < 0)
            goto cleanup;
    }
    if (make_button(&exit_button, 20, 708, 440, 72, "EXIT TO MENU",
                    EXIT_KEY, rgb(255, 135, 0), rgb(255, 255, 255)) < 0)
        goto cleanup;
    show_status("READY", "CHECK does not print paper");
    XuiClearKey();
    while (XuiIsRunning()) {
        if (!XuiHasKey()) {
            usleep(20000);
            continue;
        }
        key = XuiGetKey();
        if (key == EXIT_KEY || key == XUI_KEYCANCEL) {
            result = 0;
            break;
        }
        if (key == ACTION_KEY + CHECK)
            check_printer();
        else if (key == ACTION_KEY + FONTS)
            list_fonts();
        else if (key >= ACTION_KEY && key < ACTION_KEY + ACTION_COUNT)
            print_test((enum Action)(key - ACTION_KEY));
    }

cleanup:
    for (i = 0; i < ACTION_COUNT; ++i)
        if (actions[i].window)
            XuiDestroyWindow(actions[i].window);
    if (exit_button.window)
        XuiDestroyWindow(exit_button.window);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    return result;
}
