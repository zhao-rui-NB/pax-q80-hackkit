/* Live camera preview and one saved photo on the Q80. */
#include <stdio.h>
#include <unistd.h>
#include <xui.h>

#define PHOTO_PATH "/data/app/camera_lab/latest.png"
#define CAPTURE_KEY 1001
#define LIVE_KEY 1002
#define EXIT_KEY 1003

/* This Q80's XUI library accepts 1 for 640x480 and 1 for 75% zoom. */
#define CAMERA_640_480 1
#define PREVIEW_75 1

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static XuiWindow *screen;
static XuiWindow *camera;
static XuiWindow *photo_canvas;
static XuiFont *font;
static XuiImg *photo_image;
static Button capture_button;
static Button live_button;
static Button exit_button;

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void status(const char *message)
{
    XuiCanvasDrawRect(screen, 20, 549, 440, 57, rgb(47, 55, 64), 1, 1);
    XuiCanvasDrawText(screen, 32, 559, 31, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)message);
}

static int make_button(Button *button, int x, int width, const char *label,
                       int key, int background, int pressed_background)
{
    button->window = XuiCreateButton(screen, x, 672, width, 88);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = rgb(255, 255, 255);
    button->normal.text_font = font;
    button->normal.text_x = 13;
    button->normal.text_y = 23;
    button->normal.text_height = 32;
    button->pressed = button->normal;
    button->pressed.btn_bg = pressed_background;
    if (XuiButtonSetStat(button->window, XUI_BTN_NORMAL,
                         &button->normal) < 0 ||
        XuiButtonSetStat(button->window, XUI_BTN_PRESSED,
                         &button->pressed) < 0 ||
        XuiButtonSetKey(button->window, key) < 0)
        return -1;
    XuiShowWindow(button->window, 1, 0);
    return 0;
}

static void capture_photo(void)
{
    XuiImg *captured;
    XuiImg *loaded;
    int saved;

    if (!camera) {
        status("CAMERA NOT READY");
        return;
    }
    captured = XuiCameraCapture(camera);
    if (!captured) {
        status("CAPTURE FAILED");
        puts("Camera capture failed");
        fflush(stdout);
        return;
    }
    saved = XuiImgSaveToFile(captured, PHOTO_PATH);
    XuiImgFree(captured);
    if (saved < 0) {
        status("SAVE FAILED");
        printf("Camera save failed: %d\n", saved);
        fflush(stdout);
        return;
    }
    loaded = XuiImgLoadFromFile(PHOTO_PATH);
    if (!loaded || XuiImgResize(loaded, 460, 345) < 0) {
        if (loaded)
            XuiImgFree(loaded);
        status("PHOTO VIEW FAILED");
        puts("Camera photo view failed");
        fflush(stdout);
        return;
    }
    if (photo_image)
        XuiImgFree(photo_image);
    photo_image = loaded;
    XuiCanvasSetBackground(photo_canvas, XUI_BG_NORMAL, 0, rgb(31, 38, 45));
    if (XuiCanvasDrawImg(photo_canvas, 10, 17, 460, 345,
                         XUI_BG_NORMAL, photo_image) < 0) {
        status("PHOTO VIEW FAILED");
        return;
    }
    XuiShowWindow(camera, 0, 0);
    XuiShowWindow(photo_canvas, 1, 0);
    status("PHOTO SAVED");
    puts("Camera photo saved");
    fflush(stdout);
}

int app_main(int argc, char **argv)
{
    int key, result = 1;
    if (XuiOpen(argc, argv) < 0)
        return 1;
    screen = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!screen || !font)
        goto cleanup;

    XuiCanvasSetBackground(screen, XUI_BG_NORMAL, 0, rgb(8, 11, 15));
    XuiCanvasDrawRect(screen, 20, 20, 8, 80, rgb(0, 201, 52), 0, 1);
    XuiCanvasDrawText(screen, 44, 17, 52, font, XUI_BOLD,
                      rgb(255, 255, 255), "Q80 CAMERA LAB");
    XuiCanvasDrawText(screen, 44, 81, 24, font, XUI_BOLD,
                      rgb(194, 207, 212), "LIVE PREVIEW / PHOTO");
    XuiCanvasDrawRect(screen, 0, 142, 480, 380, rgb(31, 38, 45), 0, 1);
    photo_canvas = XuiCreateCanvas(screen, 0, 142, 480, 380);
    if (!photo_canvas)
        goto cleanup;
    XuiCanvasSetBackground(photo_canvas, XUI_BG_NORMAL, 0,
                           rgb(31, 38, 45));
    XuiShowWindow(photo_canvas, 0, 0);

    if (make_button(&capture_button, 20, 170, "CAPTURE", CAPTURE_KEY,
                    rgb(0, 201, 52), rgb(0, 153, 39)) < 0 ||
        make_button(&live_button, 200, 114, "LIVE", LIVE_KEY,
                    rgb(55, 63, 72), rgb(80, 90, 101)) < 0 ||
        make_button(&exit_button, 324, 136, "EXIT", EXIT_KEY,
                    rgb(255, 135, 0), rgb(205, 100, 0)) < 0)
        goto cleanup;

    camera = XuiCreateCamera(screen, 0, 142, 0,
                             CAMERA_640_480, PREVIEW_75);
    if (camera) {
        XuiShowWindow(camera, 1, 0);
        status("CAMERA READY");
        puts("Camera preview started");
    } else {
        status("CAMERA OPEN FAILED");
        puts("Camera preview failed");
    }
    fflush(stdout);

    XuiClearKey();
    while (XuiIsRunning()) {
        if (!XuiHasKey()) {
            usleep(10000);
            continue;
        }
        key = XuiGetKey();
        if (key == XUI_KEYCANCEL || key == EXIT_KEY) {
            result = 0;
            break;
        }
        if (key == CAPTURE_KEY)
            capture_photo();
        else if (key == LIVE_KEY && camera) {
            XuiShowWindow(photo_canvas, 0, 0);
            XuiShowWindow(camera, 1, 0);
            status("LIVE PREVIEW");
        }
    }

cleanup:
    if (camera)
        XuiDestroyWindow(camera);
    if (exit_button.window)
        XuiDestroyWindow(exit_button.window);
    if (live_button.window)
        XuiDestroyWindow(live_button.window);
    if (capture_button.window)
        XuiDestroyWindow(capture_button.window);
    if (photo_canvas)
        XuiDestroyWindow(photo_canvas);
    if (photo_image)
        XuiImgFree(photo_image);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    if (result != 0)
        puts("Camera Lab could not start");
    return result;
}
