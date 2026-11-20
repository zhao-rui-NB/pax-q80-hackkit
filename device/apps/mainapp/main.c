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
#define ROWS 4
#define APP_KEY 1000
#define PREV_KEY 1100
#define NEXT_KEY 1101
#define REFRESH_KEY 1102
#define TM_KEY 1103

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static ST_APP_INFO apps[MAX_APPS];
static int app_count;
static int page;
static XuiWindow *canvas;
static XuiFont *font;
static Button rows[ROWS];
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
}

static int button(Button *item, int x, int y, int width, int height,
                  char *label, int key, int background, int size)
{
    item->window = XuiCreateButton(canvas, x, y, width, height);
    if (!item->window)
        return -1;
    item->normal = (XuiButtonStat){0};
    item->normal.btn_round = 1;
    item->normal.btn_bg = background;
    item->normal.text = label;
    item->normal.text_fg = color(255, 255, 255);
    item->normal.text_font = font;
    item->normal.text_x = 16;
    item->normal.text_y = (height - size) / 2;
    item->normal.text_height = size;
    item->pressed = item->normal;
    item->pressed.btn_bg = color(55, 135, 175);
    if (XuiButtonSetStat(item->window, XUI_BTN_NORMAL, &item->normal) < 0 ||
        XuiButtonSetStat(item->window, XUI_BTN_PRESSED, &item->pressed) < 0 ||
        XuiButtonSetKey(item->window, key) < 0)
        return -1;
    XuiShowWindow(item->window, 1, 0);
    return 0;
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
    int i, index;
    char title[64];
    for (i = 0; i < ROWS; ++i)
        destroy(&rows[i]);
    XuiCanvasSetBackground(canvas, XUI_BG_NORMAL, 0, color(18, 26, 36));
    XuiCanvasDrawText(canvas, 24, 22, 44, font, XUI_TEXT_NORMAL,
                      color(255, 255, 255), "APPS");
    snprintf(title, sizeof(title), "%d installed  |  page %d", app_count, page + 1);
    XuiCanvasDrawText(canvas, 24, 76, 24, font, XUI_TEXT_NORMAL,
                      color(120, 205, 225), title);
    for (i = 0; i < ROWS; ++i) {
        index = page * ROWS + i;
        if (index >= app_count)
            break;
        /* Use the ID as the stable label; app names can be much longer. */
        if (button(&rows[i], 20, 140 + i * 113, 440, 98,
                   apps[index].Id, APP_KEY + i, color(42, 67, 88), 34) < 0)
            return -1;
    }
    if (app_count == 0)
        XuiCanvasDrawText(canvas, 24, 210, 30, font, XUI_TEXT_NORMAL,
                          color(230, 230, 230), "No apps found");
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
    if (button(&prev_button, 20, 610, 135, 76, "PREV", PREV_KEY,
               color(53, 60, 74), 27) < 0 ||
        button(&refresh_button, 172, 610, 135, 76, "SCAN", REFRESH_KEY,
               color(53, 60, 74), 27) < 0 ||
        button(&next_button, 324, 610, 135, 76, "NEXT", NEXT_KEY,
               color(53, 60, 74), 27) < 0 ||
        button(&tm_button, 20, 704, 440, 78, "SYSTEM SETTINGS (TM)", TM_KEY,
               color(90, 55, 48), 27) < 0)
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
                XuiCanvasDrawText(canvas, 24, 584, 18, font, XUI_TEXT_NORMAL,
                                  color(255, 160, 130), "Cannot pause menu");
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
                XuiCanvasDrawText(canvas, 24, 584, 18, font, XUI_TEXT_NORMAL,
                                  color(255, 160, 130), status);
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
