/* Simple Q80 hardware test with touch tabs and a physical Cancel exit. */
#include <osal.h>
#include <stdio.h>
#include <unistd.h>
#include <xui.h>

#define WIDTH 480
#define HEIGHT 800
#define TABS_Y 710
#define TAB_KEY 1001
#define EXIT_KEY 1010
#define ITEM_KEY 1020

enum Page { KEY, TOUCH, LCD, BEEP, POWER, PAGE_COUNT };

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static XuiWindow *root;
static XuiWindow *body;
static XuiFont *font;
static Button tabs[PAGE_COUNT];
static Button exit_button;
static Button squares[9];
static Button tones[7];
static int touched[9];
static int page = KEY;
static int last_key = -1;
static int last_tone;

static const char *const tab_names[PAGE_COUNT] = {
    "KEY", "TOUCH", "LCD", "BEEP", "POWER"
};
static const char *const digits[9] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9"
};
static const char *const battery_names[8] = {
    "0-5%", "5-15%", "15-40%", "40-70%", "70-100%",
    "CHARGING", "FULL", "NO BATTERY"
};

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void text(int x, int y, int size, int color, const char *value)
{
    XuiCanvasDrawText(body, x, y, size, font, XUI_TEXT_NORMAL,
                      color, (char *)value);
}

static int make_button(Button *button, XuiWindow *parent, int x, int y,
                       int width, int height, const char *label, int key,
                       int background, int text_size)
{
    button->window = XuiCreateButton(parent, x, y, width, height);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = rgb(255, 255, 255);
    button->normal.text_font = font;
    button->normal.text_x = 10;
    button->normal.text_y = (height - text_size) / 2;
    button->normal.text_height = text_size;
    button->pressed = button->normal;
    button->pressed.btn_bg = rgb(65, 125, 165);
    if (XuiButtonSetStat(button->window, XUI_BTN_NORMAL, &button->normal) < 0 ||
        XuiButtonSetStat(button->window, XUI_BTN_PRESSED, &button->pressed) < 0 ||
        XuiButtonSetKey(button->window, key) < 0)
        return -1;
    XuiShowWindow(button->window, 1, 0);
    return 0;
}

static void show_items(void)
{
    int i;
    for (i = 0; i < 9; ++i)
        XuiShowWindow(squares[i].window, page == TOUCH, 0);
    for (i = 0; i < 7; ++i)
        XuiShowWindow(tones[i].window, page == BEEP, 0);
}

static void draw_page(void)
{
    char line[64];
    int i;
    static const int colors[6] = {
        0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
        0xFFFFFFFF, 0xFF808080, 0xFF000000
    };
    static const char *const color_names[6] = {
        "RED", "GREEN", "BLUE", "WHITE", "GRAY", "BLACK"
    };

    XuiCanvasSetBackground(body, XUI_BG_NORMAL, 0, rgb(18, 24, 32));
    text(20, 22, 38, rgb(255, 255, 255), "HARDWARE TEST");
    text(20, 75, 27, rgb(95, 210, 230), tab_names[page]);
    XuiCanvasDrawRect(body, 20, 113, 440, 2, rgb(80, 95, 110), 0, 1);

    for (i = 0; i < PAGE_COUNT; ++i) {
        tabs[i].normal.btn_bg = rgb(i == page ? 30 : 48,
                                    i == page ? 105 : 58,
                                    i == page ? 145 : 70);
        XuiButtonSetStat(tabs[i].window, XUI_BTN_NORMAL, &tabs[i].normal);
    }
    show_items();

    if (page == KEY) {
        text(24, 160, 35, rgb(240, 240, 240), "PRESS ANY KEY");
        if (last_key >= 0) {
            snprintf(line, sizeof(line), "CODE %d", last_key);
            text(24, 280, 80, rgb(105, 225, 165), line);
        } else {
            text(24, 300, 30, rgb(155, 170, 185), "Waiting for key...");
        }
        text(24, 545, 28, rgb(230, 195, 90), "X / CANCEL = EXIT");
    } else if (page == TOUCH) {
        text(24, 130, 30, rgb(240, 240, 240), "TAP EVERY SQUARE");
        for (i = 0; i < 9; ++i) {
            squares[i].normal.btn_bg = touched[i] ? rgb(35, 130, 85)
                                                   : rgb(42, 55, 72);
            XuiButtonSetStat(squares[i].window, XUI_BTN_NORMAL,
                             &squares[i].normal);
        }
    } else if (page == LCD) {
        text(24, 130, 30, rgb(240, 240, 240), "CHECK COLORS / PIXELS");
        for (i = 0; i < 6; ++i) {
            int x = 24 + (i % 3) * 148;
            int y = 190 + (i / 3) * 210;
            XuiCanvasDrawRect(body, x, y, 132, 145, colors[i], 0, 1);
            text(x, y + 155, 25, rgb(230, 230, 230), color_names[i]);
        }
    } else if (page == BEEP) {
        text(24, 130, 30, rgb(240, 240, 240), "TAP A TONE");
        if (last_tone) {
            snprintf(line, sizeof(line), "TONE %d", last_tone);
            text(24, 565, 36, rgb(105, 225, 165), line);
        }
    } else {
        int battery = OsCheckBattery();
        int supply = OsCheckPowerSupply();
        text(24, 145, 30, rgb(240, 240, 240), "BATTERY LEVEL");
        if (battery >= 0 && battery < 8)
            snprintf(line, sizeof(line), "%s (%d)", battery_names[battery], battery);
        else if (battery == -1015) /* ERR_SYS_NOT_SUPPORT: no battery gauge. */
            snprintf(line, sizeof(line), "NOT SUPPORTED");
        else
            snprintf(line, sizeof(line), "ERROR %d", battery);
        text(24, 205, 48, rgb(105, 225, 165), line);
        text(24, 325, 30, rgb(240, 240, 240), "POWER SOURCE");
        snprintf(line, sizeof(line), "%s (%d)",
                 supply == 1 ? "ADAPTER" : supply == POWER_USB ? "USB" :
                 supply == POWER_BATTERY ? "BATTERY" :
                 supply == 4 ? "WIRELESS" : "UNKNOWN", supply);
        text(24, 385, 42, rgb(105, 225, 165), line);
        text(24, 535, 25, rgb(155, 170, 185), "OSAL POWER STATUS");
    }
}

static void destroy_buttons(Button *buttons, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        if (buttons[i].window)
            XuiDestroyWindow(buttons[i].window);
}

int app_main(int argc, char **argv)
{
    int i;
    int key;
    int result = 1;
    if (XuiOpen(argc, argv) < 0)
        return 1;
    root = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    body = root ? XuiCreateCanvas(root, 0, 0, WIDTH, TABS_Y) : 0;
    if (!font || !body)
        goto cleanup;
    XuiShowWindow(body, 1, 0);

    for (i = 0; i < PAGE_COUNT; ++i)
        if (make_button(&tabs[i], root, i * 96, TABS_Y, 96, 90,
                        tab_names[i], TAB_KEY + i, rgb(48, 58, 70), 23) < 0)
            goto cleanup;
    if (make_button(&exit_button, body, 360, 67, 100, 48,
                    "EXIT", EXIT_KEY, rgb(135, 45, 45), 24) < 0)
        goto cleanup;
    for (i = 0; i < 9; ++i)
        if (make_button(&squares[i], body, 24 + (i % 3) * 145,
                        195 + (i / 3) * 135, 130, 120,
                        digits[i], ITEM_KEY + i, rgb(42, 55, 72), 48) < 0)
            goto cleanup;
    for (i = 0; i < 7; ++i)
        if (make_button(&tones[i], body, 24 + (i % 4) * 110,
                        230 + (i / 4) * 165, 98, 125,
                        digits[i], ITEM_KEY + i, rgb(45, 80, 120), 48) < 0)
            goto cleanup;

    XuiClearKey();
    draw_page();
    while (XuiIsRunning()) {
        if (XuiHasKey()) {
            key = XuiGetKey();
            if (key == XUI_KEYCANCEL || key == EXIT_KEY)
                break;
            if (key >= TAB_KEY && key < TAB_KEY + PAGE_COUNT) {
                page = key - TAB_KEY;
                draw_page();
            } else if (key >= ITEM_KEY && key < ITEM_KEY + 9) {
                if (page == TOUCH) {
                    touched[key - ITEM_KEY] = 1;
                    draw_page();
                } else if (page == BEEP && key < ITEM_KEY + 7) {
                    last_tone = key - ITEM_KEY + 1;
                    OsBeep(last_tone, 150);
                    draw_page();
                }
            } else if (page == KEY) {
                last_key = key;
                draw_page();
            }
        }
        usleep(20000);
    }
    result = 0;

cleanup:
    destroy_buttons(tones, 7);
    destroy_buttons(squares, 9);
    destroy_buttons(&exit_button, 1);
    destroy_buttons(tabs, PAGE_COUNT);
    if (body)
        XuiDestroyWindow(body);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    return result;
}
