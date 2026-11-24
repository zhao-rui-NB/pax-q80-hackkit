/* Interactive Q80 display test using the device's XUI and OSAL libraries. */
#include <osal.h>
#include <stdio.h>
#include <unistd.h>
#include <xui.h>

#define FIRST_SWATCH 600
#define EXIT_BUTTON 610

static XuiWindow *canvas;
static XuiFont *font;

static const struct {
    char *name;
    unsigned int hex;
    int dark_text;
} swatches[] = {
    {"RED",     0xe45055, 1},
    {"GREEN",   0x46bf90, 1},
    {"BLUE",    0x4d88e5, 1},
    {"WHITE",   0xffffff, 1},
    {"BLACK",   0x000000, 0},
};

#define SWATCH_COUNT (sizeof(swatches) / sizeof(swatches[0]))

static int color(unsigned int hex)
{
    return (int)XuiColor(hex & 255, (hex >> 8) & 255, (hex >> 16) & 255, 255);
}

static void text(int x, int y, int size, unsigned int hex, XuiTextStyle style, char *label)
{
    XuiCanvasDrawText(canvas, x, y, size, font, style, color(hex), label);
}

static void panel(int x, int y, int width, int height, unsigned int hex)
{
    XuiCanvasDrawRect(canvas, x, y, width, height, color(hex), 1, 1);
}

static XuiWindow *make_button(int x, int y, int width, int height, int key,
                              char *label, unsigned int bg, unsigned int fg, int size)
{
    XuiWindow *window = XuiCreateButton(canvas, x, y, width, height);
    XuiButtonStat normal = {0}, pressed;
    int text_width = XuiTextWidthEx(font, size, XUI_BOLD, label);

    if (!window) return NULL;
    normal.btn_round = 1;
    normal.btn_bg = color(bg);
    normal.text = label;
    normal.text_fg = color(fg);
    normal.text_font = font;
    normal.text_x = text_width > 0 && text_width < width ? (width - text_width) / 2 : 4;
    normal.text_y = (height - size) / 2;
    normal.text_height = size;
    pressed = normal;
    pressed.btn_bg = color(0x34445c);
    pressed.text_fg = color(0xffffff);
    if (XuiButtonSetStat(window, XUI_BTN_NORMAL, &normal) < 0 ||
        XuiButtonSetStat(window, XUI_BTN_PRESSED, &pressed) < 0 ||
        XuiButtonSetKey(window, key) < 0) {
        XuiDestroyWindow(window);
        return NULL;
    }
    XuiShowWindow(window, 1, 0);
    return window;
}

static void show_color(unsigned int index)
{
    unsigned int fg = swatches[index].dark_text ? 0x101826 : 0xffffff;
    char sample[32];
    snprintf(sample, sizeof(sample), "%s  #%06X", swatches[index].name, swatches[index].hex);
    panel(28, 510, 424, 100, swatches[index].hex);
    text(46, 544, 26, fg, XUI_BOLD, sample);
}

int app_main(int argc, char **argv)
{
    XuiWindow *buttons[SWATCH_COUNT + 1] = {0};
    unsigned int i;
    int result = 1;
    int width = 480, height = 800;
    char display_size[32];

    if (XuiOpen(argc, argv) < 0) return 1;
    canvas = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!canvas || !font) goto cleanup;

    OsScrGetSize(&width, &height);
    snprintf(display_size, sizeof(display_size), "%d x %d", width, height);
    XuiCanvasSetBackground(canvas, XUI_BG_NORMAL, 0, color(0x0f1724));
    panel(20, 26, 6, 66, 0x54d9bb);
    text(40, 26, 40, 0xf5f9ff, XUI_BOLD, "Display Test");
    text(42, 76, 22, 0xa5b8d4, XUI_TEXT_NORMAL, display_size);

    panel(20, 121, 440, 255, 0x202d42);
    text(39, 143, 20, 0x87decf, XUI_BOLD, "BUILT-IN FONT");
    text(39, 183, 37, 0xffffff, XUI_TEXT_NORMAL, "中文字顯示測試");
    text(39, 244, 32, 0xf5f9ff, XUI_BOLD, "Hello Q80");
    text(39, 304, 31, 0xd1ddef, XUI_TEXT_NORMAL, "0123456789");

    text(25, 397, 23, 0xf5f9ff, XUI_BOLD, "COLOR");
    for (i = 0; i < SWATCH_COUNT; ++i) {
        buttons[i] = make_button(28 + i * 86, 439, 80, 52, FIRST_SWATCH + i,
                                 swatches[i].name, swatches[i].hex,
                                 swatches[i].dark_text ? 0x101826 : 0xffffff, 16);
        if (!buttons[i]) goto cleanup;
    }
    show_color(2);
    text(27, 634, 21, 0xa5b8d4, XUI_TEXT_NORMAL, "Tap a color or press 1-5");
    buttons[SWATCH_COUNT] = make_button(20, 704, 440, 64, EXIT_BUTTON,
                                       "EXIT", 0x34445c, 0xffffff, 25);
    if (!buttons[SWATCH_COUNT]) goto cleanup;

    XuiClearKey();
    while (XuiIsRunning()) {
        int key;
        if (!XuiHasKey()) {
            usleep(12000);
            continue;
        }
        key = XuiGetKey();
        if (key == EXIT_BUTTON || key == XUI_KEYCANCEL) break;
        if (key >= XUI_KEY1 && key < XUI_KEY1 + (int)SWATCH_COUNT)
            show_color((unsigned int)(key - XUI_KEY1));
        else if (key >= FIRST_SWATCH && key < FIRST_SWATCH + (int)SWATCH_COUNT)
            show_color((unsigned int)(key - FIRST_SWATCH));
    }
    result = 0;

cleanup:
    for (i = 0; i <= SWATCH_COUNT; ++i)
        if (buttons[i]) XuiDestroyWindow(buttons[i]);
    if (font) XuiDestroyFont(font);
    XuiClose();
    return result;
}
