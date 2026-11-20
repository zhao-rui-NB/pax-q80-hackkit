/* Small MAINAPP launcher for the 480x800 Q80 screen.
 * keyman starts this program through runapp MAINAPP --system.
 */
#include <osal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <xui.h>

#define WIDTH 480
#define HEIGHT 800
#define MAX_APPS 32
#define ROWS 5
#define ROW_TOP 168
#define ROW_STEP 87
#define ROW_HEIGHT 75
#define APP_KEY 1000
#define PREV_KEY 1100
#define NEXT_KEY 1101
#define REFRESH_KEY 1102
#define TM_KEY 1103

typedef struct {
    XuiWindow *window;
    XuiImg *image;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static ST_APP_INFO apps[MAX_APPS];
static int app_count;
static int page;
static XuiWindow *canvas;
static XuiFont *font;
static Button rows[ROWS];
static char row_labels[ROWS][96];
static Button prev_button, next_button, refresh_button, tm_button;

static int color(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void destroy(Button *button)
{
    if (button->window)
        XuiDestroyWindow(button->window);
    button->window = NULL;
    if (button->image)
        XuiImgFree(button->image);
    button->image = NULL;
}

static int button(Button *item, int x, int y, int width, int height,
                  char *label, int key, int background, int pressed_background,
                  int foreground, int size, int text_x, XuiImg *image)
{
    item->image = image;
    item->window = XuiCreateButton(canvas, x, y, width, height);
    if (!item->window)
        return -1;
    item->normal = (XuiButtonStat){0};
    item->normal.btn_round = 1;
    item->normal.btn_bg = background;
    item->normal.text = image ? "" : label;
    item->normal.text_fg = foreground;
    item->normal.text_font = font;
    item->normal.text_x = text_x;
    item->normal.text_y = (height - size) / 2;
    item->normal.text_height = size;
    item->normal.img = image;
    item->normal.img_style = XUI_BG_NORMAL;
    item->pressed = item->normal;
    item->pressed.btn_bg = pressed_background;
    if (image) {
        item->pressed.img = NULL;
        item->pressed.text = label;
    }
    if (XuiButtonSetStat(item->window, XUI_BTN_NORMAL, &item->normal) < 0 ||
        XuiButtonSetStat(item->window, XUI_BTN_PRESSED, &item->pressed) < 0 ||
        XuiButtonSetKey(item->window, key) < 0)
        return -1;
    XuiShowWindow(item->window, 1, 0);
    return 0;
}

static XuiImg *render_label(const char *label, int width, int height,
                            int size, int background, int foreground,
                            int text_x)
{
    XuiWindow *label_canvas = XuiCreateCanvas(canvas, 0, 0, width, height);
    XuiImg *image;
    if (!label_canvas)
        return NULL;
    XuiCanvasSetBackground(label_canvas, XUI_BG_NORMAL, 0, background);
    XuiCanvasDrawText(label_canvas, text_x, (height - size) / 2,
                      size, font, XUI_BOLD, foreground, (char *)label);
    image = XuiCaptureCanvas(label_canvas, 0, 0, width, height);
    XuiDestroyWindow(label_canvas);
    return image;
}

static int action_button(Button *item, int x, int y, int width, int height,
                         char *label, int key, int background,
                         int pressed_background, int size)
{
    int text_x = (width - XuiTextWidthEx(font, size, XUI_BOLD, label)) / 2;
    XuiImg *image;
    if (text_x < 10)
        text_x = 10;
    image = render_label(label, width, height, size, background,
                         color(255, 255, 255), text_x);
    return button(item, x, y, width, height, label, key, background,
                  pressed_background, color(255, 255, 255),
                  size, text_x, image);
}

static void read_apps(void)
{
    int i, shown = 0, count;
    count = OsGetAppInfo(apps, MAX_APPS);
    for (i = 0; i < count; ++i) {
        apps[i].Id[sizeof(apps[i].Id) - 1] = 0;
        apps[i].Name[sizeof(apps[i].Name) - 1] = 0;
        if (strcmp(apps[i].Id, "MAINAPP"))
            apps[shown++] = apps[i];
    }
    app_count = shown;
    if (page * ROWS >= app_count)
        page = 0;
    printf("OsGetAppInfo=%d\n", count);
    fflush(stdout);
}

static int draw(void)
{
    int i, index, label_size;
    XuiImg *label_image;
    char title[64];
    for (i = 0; i < ROWS; ++i)
        destroy(&rows[i]);
    XuiCanvasSetBackground(canvas, XUI_BG_NORMAL, 0, color(8, 11, 15));
    XuiCanvasDrawRect(canvas, 18, 17, 9, 85, color(0, 201, 52), 0, 1);
    XuiCanvasDrawRect(canvas, 354, 31, 30, 9, color(225, 22, 27), 0, 1);
    XuiCanvasDrawRect(canvas, 389, 31, 30, 9, color(255, 135, 0), 0, 1);
    XuiCanvasDrawRect(canvas, 424, 31, 30, 9, color(0, 201, 52), 0, 1);
    XuiCanvasDrawText(canvas, 42, 8, 61, font, XUI_BOLD,
                      color(255, 255, 255), "Q80");
    XuiCanvasDrawText(canvas, 42, 75, 28, font, XUI_BOLD,
                      color(255, 255, 255), "APPLICATION LAUNCHER");
    snprintf(title, sizeof(title), "%d APPS   |   PAGE %d / %d",
             app_count, page + 1,
             app_count ? (app_count + ROWS - 1) / ROWS : 1);
    XuiCanvasDrawRect(canvas, 20, 121, 440, 38,
                      color(52, 60, 69), 1, 1);
    XuiCanvasDrawText(canvas, 32, 123, 29, font, XUI_BOLD,
                      color(255, 255, 255), title);
    for (i = 0; i < ROWS; ++i) {
        index = page * ROWS + i;
        if (index >= app_count)
            break;
        /* Use the ID as the stable label; app names can be much longer. */
        snprintf(row_labels[i], sizeof(row_labels[i]), "%02d   %s",
                 index + 1, apps[index].Id);
        label_size = 38;
        while (label_size > 24 &&
               XuiTextWidthEx(font, label_size, XUI_BOLD,
                              row_labels[i]) > 405)
            --label_size;
        XuiCanvasDrawRect(canvas, 20, ROW_TOP + i * ROW_STEP + 4,
                          440, ROW_HEIGHT, color(62, 69, 77), 1, 1);
        label_image = render_label(row_labels[i], 440, ROW_HEIGHT,
                                   label_size, color(255, 255, 255),
                                   color(16, 16, 16), 18);
        if (button(&rows[i], 20, ROW_TOP + i * ROW_STEP, 440, ROW_HEIGHT,
                   row_labels[i], APP_KEY + i, color(255, 255, 255),
                   color(222, 229, 226), color(16, 16, 16),
                   label_size, 18, label_image) < 0)
            return -1;
    }
    if (app_count == 0)
        XuiCanvasDrawText(canvas, 24, 228, 36, font, XUI_BOLD,
                          color(255, 255, 255), "No apps found");
    return 0;
}

int main(int argc, char **argv)
{
    int key, index, launch_result, result = 1;
    char status[80];
    if (XuiOpen(argc, argv) < 0)
        return 1;
    canvas = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!canvas || !font)
        goto cleanup;
    if (action_button(&prev_button, 20, 620, 135, 70, "PREV", PREV_KEY,
                      color(55, 63, 72), color(80, 90, 101), 32) < 0 ||
        action_button(&refresh_button, 172, 620, 135, 70, "SCAN", REFRESH_KEY,
                      color(0, 201, 52), color(0, 153, 39), 32) < 0 ||
        action_button(&next_button, 324, 620, 135, 70, "NEXT", NEXT_KEY,
                      color(55, 63, 72), color(80, 90, 101), 32) < 0 ||
        action_button(&tm_button, 20, 706, 440, 75, "SYSTEM SETTINGS (TM)",
                      TM_KEY, color(255, 135, 0), color(205, 100, 0), 32) < 0)
        goto cleanup;
    read_apps();
    if (draw() < 0)
        goto cleanup;
    XuiClearKey();
    while (XuiIsRunning()) {
        if (!XuiHasKey()) {
            usleep(20000);
            continue;
        }
        key = XuiGetKey();
        if (key == TM_KEY || key == XUI_KEYCANCEL) {
            /* A normal MAINAPP exit enters TM when rt.sys.mainapp.restart=0. */
            OsRegSetValue("rt.sys.mainapp.restart", "0");
            result = 0;
            break;
        }
        if (key == REFRESH_KEY) {
            read_apps();
            draw();
        } else if (key == PREV_KEY && page > 0) {
            --page;
            draw();
        } else if (key == NEXT_KEY && (page + 1) * ROWS < app_count) {
            ++page;
            draw();
        } else if (key >= APP_KEY && key < APP_KEY + ROWS) {
            index = page * ROWS + key - APP_KEY;
            if (index >= app_count || !strcmp(apps[index].Id, "MAINAPP"))
                continue;
            printf("Open app: %s\n", apps[index].Id);
            fflush(stdout);
            /* MAINAPP keeps the stock runapp flow and yields XUI to the child. */
            if (XuiSuspend() != 0) {
                XuiCanvasDrawText(canvas, 24, 588, 25, font, XUI_BOLD,
                                  color(160, 39, 45), "Cannot pause menu");
                continue;
            }
            launch_result = OsRunApp(apps[index].Id, NULL, NULL, NULL, NULL);
            printf("OsRunApp=%d\n", launch_result);
            fflush(stdout);
            if (XuiResume() != 0)
                break;
            read_apps();
            draw();
            if (launch_result != RET_OK) {
                snprintf(status, sizeof(status), "Open failed: %d", launch_result);
                XuiCanvasDrawText(canvas, 24, 588, 25, font, XUI_BOLD,
                                  color(160, 39, 45), status);
            }
            XuiClearKey();
        }
    }

cleanup:
    for (key = 0; key < ROWS; ++key)
        destroy(&rows[key]);
    destroy(&prev_button);
    destroy(&refresh_button);
    destroy(&next_button);
    destroy(&tm_button);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    return result;
}

/* The unsigned test build is loaded as a DLL by runapp on this Q80. */
int app_main(int argc, char **argv)
{
    return main(argc, argv);
}
