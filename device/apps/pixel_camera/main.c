/* Q80 camera and full-width monochrome thermal photo preview. */
#include <osal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <xui.h>

#include "raster.h"

#define PIXEL_PATH "/data/app/pixel_camera/latest_pixel.png"
#define CAMERA_640_480 1
#define PREVIEW_75 1
#define PRINT_WIDTH 384
#define WIDE_HEIGHT 288
#define TALL_HEIGHT 512
#define SOURCE_WIDTH 640
#define SOURCE_HEIGHT 480
#define LIVE_WIDTH 320
#define LIVE_HEIGHT 240
#define LIVE_SCREEN_WIDTH 480
#define LIVE_SCREEN_HEIGHT 360
#define VIEW_Y 74
#define VIEW_HEIGHT TALL_HEIGHT
#define KEYPAD_Y 594
#define ROW_STEP 47
#define KEY_WIDTH 140
#define KEY_HEIGHT 42
#define ROW_BYTES (PRINT_WIDTH / 8)
#define CHUNK_HEIGHT 144
#define MANUAL_FEED_DOTS 96
#define PACKET_SIZE (1 + CHUNK_HEIGHT * (2 + ROW_BYTES))
#define LIVE_INTERVAL_MS 100
#define ACTION_DELAY_MS 900
#define ADJUST_STEP 8
#define ADJUST_LIMIT 64

enum PhotoState { CAMERA_IDLE, CAMERA_LIVE, PHOTO_REVIEW };
enum ButtonId {
    MODE, LAYOUT, FACE, LIGHTER, DARKER, CELL, DENSITY, FEED,
    RETURN_LIVE, ACTION, EXIT, BUTTON_COUNT
};

#define BUTTON_KEY 1100

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
} Button;

static XuiWindow *screen, *camera, *photo_canvas, *live_canvas;
static XuiFont *font;
static XuiImg *photo_image, *live_image;
static Button buttons[BUTTON_COUNT];
static unsigned char snapshot[SOURCE_WIDTH * SOURCE_HEIGHT];
static unsigned char face_gray[SOURCE_WIDTH * SOURCE_HEIGHT];
static unsigned char gray[PRINT_WIDTH * TALL_HEIGHT];
static unsigned char rgba[SOURCE_WIDTH * SOURCE_HEIGHT * 4];
static unsigned char bitmap[ROW_BYTES * TALL_HEIGHT];
static unsigned char packet[PACKET_SIZE];
static enum PhotoState state = CAMERA_IDLE;
static enum PhotoLayout layout = PHOTO_WIDE;
static enum PixelMode mode = PIXEL_SHADE;
static int cell_size = 3;
static int print_density = 4;
static int adjustment, face_enabled, live_corrected, bitmap_ready, print_height;
static unsigned long last_live_ms, action_ms;

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static unsigned long millis(void)
{
    struct timeval now = {0};
    gettimeofday(&now, NULL);
    return (unsigned long)now.tv_sec * 1000 + (unsigned long)now.tv_usec / 1000;
}

static void text(int x, int y, int size, int color, const char *label)
{
    XuiCanvasDrawText(screen, x, y, size, font, XUI_BOLD, color, (char *)label);
}

static void status(const char *message)
{
    int size = 17;
    while (size > 13 && XuiTextWidthEx(font, size, XUI_BOLD, (char *)message) > 240)
        --size;
    XuiCanvasDrawRect(screen, 16, 46, 260, 24, rgb(43, 53, 65), 1, 1);
    text(25, 46 + (24 - size) / 2, size, rgb(255, 255, 255), message);
}

static void set_button(enum ButtonId id, const char *label, int background, int foreground)
{
    Button *button = &buttons[id];
    XuiButtonStat pressed;
    int size = 21;
    int width = XuiTextWidthEx(font, size, XUI_TEXT_NORMAL, (char *)label);
    while (width > button->window->width - 10 && size > 15)
        width = XuiTextWidthEx(font, --size, XUI_TEXT_NORMAL, (char *)label);
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = foreground;
    button->normal.text_font = font;
    button->normal.text_x = width > 0 && width < button->window->width
                            ? (button->window->width - width) / 2 : 4;
    button->normal.text_y = (button->window->height - size) / 2;
    button->normal.text_height = size;
    pressed = button->normal;
    pressed.btn_bg = rgb(191, 207, 218);
    pressed.text_fg = rgb(16, 24, 32);
    XuiButtonSetStat(button->window, XUI_BTN_NORMAL, &button->normal);
    XuiButtonSetStat(button->window, XUI_BTN_PRESSED, &pressed);
}

static int make_button(enum ButtonId id, int x, int y, int width, int height)
{
    Button *button = &buttons[id];
    button->window = XuiCreateButton(screen, x, y, width, height);
    if (!button->window || XuiButtonSetKey(button->window, BUTTON_KEY + id) < 0)
        return -1;
    XuiShowWindow(button->window, 1, 0);
    return 0;
}

static void draw_controls(void)
{
    int blue = rgb(38, 76, 112), white = rgb(255, 255, 255);
    int ink = rgb(16, 24, 32), gray_color = rgb(58, 70, 84);
    char value[48];
    static const char *const mode_labels[] = {"1 SHADE", "1 CLUSTER", "1 BOLD"};
    static const char *const cell_labels[] = {"5 2x2", "5 3x3", "5 4x4"};
    const char *density_label = print_density == 1 ? "7 DENS 1" :
                                print_density == 3 ? "7 DENS 3" : "7 DENS 4";
    set_button(MODE, mode_labels[mode], blue, white);
    set_button(CELL, cell_labels[cell_size - 2], mode == PIXEL_BOLD ? gray_color : blue, white);
    set_button(LAYOUT, layout == PHOTO_TALL ? "2 TALL" : "2 WIDE", blue, white);
    set_button(FACE, "3 FACE", face_enabled ? blue : gray_color, white);
    set_button(DENSITY, density_label, blue, white);
    set_button(FEED, "8 FEED", gray_color, white);
    set_button(LIGHTER, "4 LIGHTER", white, ink);
    set_button(DARKER, "6 DARKER", white, ink);
    set_button(RETURN_LIVE, state == PHOTO_REVIEW ? "RETAKE" : "LIVE",
               rgb(255, 135, 0), ink);
    set_button(ACTION, state == PHOTO_REVIEW ? "PRINT" : "SNAP",
               rgb(0, 201, 52), ink);
    set_button(EXIT, "EXIT", rgb(195, 35, 49), white);
    XuiCanvasDrawRect(screen, 324, KEYPAD_Y + 2 * ROW_STEP, KEY_WIDTH, KEY_HEIGHT,
                      rgb(26, 34, 44), 1, 1);
    snprintf(value, sizeof(value), "%s %+d", mode == PIXEL_BOLD ? "THR" : "INK", adjustment);
    text(340, KEYPAD_Y + 2 * ROW_STEP + 12, 18, white, value);
    XuiCanvasDrawRect(screen, 286, 46, 178, 24, rgb(26, 34, 44), 1, 1);
    snprintf(value, sizeof(value), "384 x %d", layout == PHOTO_TALL ? TALL_HEIGHT : WIDE_HEIGHT);
    text(297, 49, 17, rgb(173, 191, 211), value);
}

static int capture_rgba(int width, int height)
{
    XuiImg *image = XuiCameraCapture(camera);
    int result = -1;
    if (!image) return -1;
    int needs_resize = image->width != width || image->height != height;
    if ((!needs_resize || XuiImgResize(image, width, height) >= 0) &&
        XuiImgToRgba(image, (char *)rgba) >= 0) {
        int x, y;
        for (y = 0; y < height; ++y) {
            for (x = 0; x < width / 2; ++x) {
                unsigned char *left = rgba + (y * width + x) * 4;
                unsigned char *right = rgba + (y * width + width - 1 - x) * 4;
                unsigned char pixel[4];
                memcpy(pixel, left, 4);
                memcpy(left, right, 4);
                memcpy(right, pixel, 4);
            }
        }
        result = 0;
    }
    XuiImgFree(image);
    return result;
}

static unsigned char luminance(const unsigned char *pixel)
{
    return (unsigned char)((77 * pixel[0] + 150 * pixel[1] + 29 * pixel[2]) / 256);
}

static int capture_gray(void)
{
    int i;
    if (capture_rgba(SOURCE_WIDTH, SOURCE_HEIGHT) < 0) return -1;
    for (i = 0; i < SOURCE_WIDTH * SOURCE_HEIGHT; ++i)
        snapshot[i] = luminance(rgba + i * 4);
    return 0;
}

static int draw_bitmap(XuiWindow *canvas, XuiImg **previous, const unsigned char *bits,
                       int width, int height)
{
    int x, y;
    XuiImg *image;
    if (width > canvas->width || height > canvas->height) return -1;
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            int pos = (y * width + x) * 4;
            unsigned char value = bits[y * (width / 8) + x / 8] & (0x80 >> (x % 8)) ? 0 : 255;
            rgba[pos] = rgba[pos + 1] = rgba[pos + 2] = value;
            rgba[pos + 3] = 255;
        }
    }
    image = XuiRgbaToImg((char *)rgba, width * height * 4, width, height);
    if (!image) return -1;
    XuiCanvasDrawRect(canvas, 0, 0, canvas->width, canvas->height, rgb(25, 33, 44), 0, 1);
    if (XuiCanvasDrawImg(canvas, (canvas->width - width) / 2,
                         (canvas->height - height) / 2, width, height,
                         XUI_BG_NORMAL, image) < 0) {
        XuiImgFree(image);
        return -1;
    }
    if (XuiImgSaveToFile(image, PIXEL_PATH) < 0)
        puts("Pixel Camera could not save the preview");
    if (*previous) XuiImgFree(*previous);
    *previous = image;
    return 0;
}

static int render_photo(void)
{
    print_height = layout == PHOTO_TALL ? TALL_HEIGHT : WIDE_HEIGHT;
    bitmap_ready = 0;
    const unsigned char *source = snapshot;
    if (face_enabled) {
        pixel_lift_center(snapshot, SOURCE_WIDTH, SOURCE_HEIGHT, face_gray);
        source = face_gray;
    }
    pixel_convert(source, SOURCE_WIDTH, SOURCE_HEIGHT, PRINT_WIDTH, print_height,
                  layout, mode, adjustment, cell_size, gray, bitmap);
    if (draw_bitmap(photo_canvas, &photo_image, bitmap, PRINT_WIDTH, print_height) < 0)
        return -1;
    bitmap_ready = 1;
    return 0;
}

static void start_live(void)
{
    if (!camera) camera = XuiCreateCamera(screen, 0, VIEW_Y + (VIEW_HEIGHT - 360) / 2, 0, CAMERA_640_480, PREVIEW_75);
    state = camera ? CAMERA_LIVE : CAMERA_IDLE;
    bitmap_ready = 0;
    XuiShowWindow(photo_canvas, 0, 0);
    XuiShowWindow(live_canvas, 0, 0);
    live_corrected = camera != NULL;
    if (camera) XuiShowWindow(camera, 0, 0);
    last_live_ms = millis();
    draw_controls();
    status(camera ? "LIVE" : "CAMERA FAILED");
}

static void snap(void)
{
    if (state != CAMERA_LIVE) return;
    status("CAPTURING");
    if (capture_gray() < 0) {
        status("SNAP FAILED");
        return;
    }
    if (render_photo() < 0) {
        status("PREVIEW FAILED");
        return;
    }
    state = PHOTO_REVIEW;
    XuiShowWindow(live_canvas, 0, 0);
    XuiShowWindow(camera, 0, 0);
    XuiShowWindow(photo_canvas, 1, 0);
    draw_controls();
    status("PHOTO READY");
    action_ms = millis();
    XuiClearKey();
}

static XuiImg *capture_live_image(void)
{
    XuiImg *image;
    if (!face_enabled) {
        image = XuiCameraCapture(camera);
        if (!image) return NULL;
        if (XuiImgResize(image, LIVE_SCREEN_WIDTH, LIVE_SCREEN_HEIGHT) >= 0 &&
            XuiImgTransform(image, XUI_FLIP_HORIZ) >= 0)
            return image;
    } else {
        int i, channel;
        if (capture_rgba(LIVE_WIDTH, LIVE_HEIGHT) < 0) return NULL;
        for (i = 0; i < LIVE_WIDTH * LIVE_HEIGHT; ++i)
            gray[i] = luminance(rgba + i * 4);
        pixel_lift_center(gray, LIVE_WIDTH, LIVE_HEIGHT, face_gray);
        for (i = 0; i < LIVE_WIDTH * LIVE_HEIGHT; ++i) {
            int lift = face_gray[i] - gray[i];
            for (channel = 0; channel < 3; ++channel) {
                int value = rgba[i * 4 + channel] + lift;
                rgba[i * 4 + channel] = (unsigned char)(value > 255 ? 255 : value);
            }
        }
        image = XuiRgbaToImg((char *)rgba, LIVE_WIDTH * LIVE_HEIGHT * 4, LIVE_WIDTH, LIVE_HEIGHT);
        if (!image) return NULL;
        if (XuiImgResize(image, LIVE_SCREEN_WIDTH, LIVE_SCREEN_HEIGHT) >= 0) return image;
    }
    XuiImgFree(image);
    return NULL;
}

static void update_live(void)
{
    unsigned long now = millis();
    XuiImg *image;
    if (state != CAMERA_LIVE || !live_corrected || now - last_live_ms < LIVE_INTERVAL_MS)
        return;
    last_live_ms = now;
    image = capture_live_image();
    if (!image) goto native_preview;
    if (XuiCanvasDrawImg(live_canvas, 0, (VIEW_HEIGHT - LIVE_SCREEN_HEIGHT) / 2,
                         LIVE_SCREEN_WIDTH, LIVE_SCREEN_HEIGHT,
                         XUI_BG_NORMAL, image) < 0) {
        XuiImgFree(image);
        goto native_preview;
    }
    if (live_image) XuiImgFree(live_image);
    live_image = image;
    XuiShowWindow(live_canvas, 1, 0);
    return;

native_preview:
    live_corrected = 0;
    XuiShowWindow(live_canvas, 0, 0);
    XuiShowWindow(camera, 1, 0);
    status("NATIVE LIVE");
}

static void settings_changed(void)
{
    draw_controls();
    if (state == PHOTO_REVIEW)
        status(render_photo() == 0 ? "PHOTO READY" : "PREVIEW FAILED");
}

static void queue_image_chunk(int first, int rows)
{
    int row, index = 1;
    packet[0] = (unsigned char)rows;
    for (row = 0; row < rows; ++row) {
        packet[index++] = 0;
        packet[index++] = ROW_BYTES;
        memcpy(packet + index, bitmap + (first + row) * ROW_BYTES, ROW_BYTES);
        index += ROW_BYTES;
    }
    OsPrnPutImage(packet);
}

static void print_photo(void)
{
    int result, start = 0;
    char message[48];
    if (!bitmap_ready) {
        status("PHOTO NOT READY");
        return;
    }
    status("PRINTING");
    result = OsPrnOpen(PRN_REAL, NULL);
    if (result == RET_OK) {
        while (start < print_height && (result = OsPrnCheck()) == RET_OK) {
            int chunk;
            OsPrnClrBuf();
            OsPrnSetGray(print_density);
            OsPrnSetIndent(0, 0);
            OsPrnSetSpace(0, 0);
            for (chunk = 0; chunk < 2 && start < print_height; ++chunk) {
                int rows = print_height - start;
                if (rows > CHUNK_HEIGHT) rows = CHUNK_HEIGHT;
                queue_image_chunk(start, rows);
                start += rows;
            }
            if (start == print_height) OsPrnFeed(48);
            result = OsPrnStart();
            if (result != RET_OK) break;
        }
        OsPrnClose();
    }
    snprintf(message, sizeof(message), result == RET_OK ? "PRINTED" : "PRINT ERR %d", result);
    status(message);
    action_ms = millis();
    XuiClearKey();
}

static void feed_paper(void)
{
    int result = OsPrnOpen(PRN_REAL, NULL);
    if (result == RET_OK) {
        result = OsPrnCheck();
        if (result == RET_OK) {
            OsPrnClrBuf();
            OsPrnFeed(MANUAL_FEED_DOTS);
            result = OsPrnStart();
        }
        OsPrnClose();
    }
    if (result == RET_OK) {
        status("PAPER FED");
    } else {
        char message[40];
        snprintf(message, sizeof(message), "FEED ERR %d", result);
        status(message);
    }
    XuiClearKey();
}

int app_main(int argc, char **argv)
{
    static const enum ButtonId grid[][3] = {
        {MODE, LAYOUT, FACE},
        {LIGHTER, CELL, DARKER},
        {DENSITY, FEED, BUTTON_COUNT},
        {EXIT, RETURN_LIVE, ACTION},
    };
    int key, result = 1, i;
    unsigned int row, col;
    int white = rgb(255, 255, 255);

    if (XuiOpen(argc, argv) < 0) return 1;
    screen = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!screen || !font) goto cleanup;
    XuiCanvasSetBackground(screen, XUI_BG_NORMAL, 0, rgb(10, 15, 22));
    XuiCanvasDrawRect(screen, 16, 8, 6, 32, rgb(0, 201, 52), 0, 1);
    text(33, 8, 33, white, "PIXEL CAMERA");
    photo_canvas = XuiCreateCanvas(screen, 0, VIEW_Y, 480, VIEW_HEIGHT);
    live_canvas = XuiCreateCanvas(screen, 0, VIEW_Y, 480, VIEW_HEIGHT);
    if (!photo_canvas || !live_canvas) goto cleanup;
    XuiShowWindow(photo_canvas, 0, 0);
    XuiShowWindow(live_canvas, 0, 0);
    XuiCanvasSetBackground(live_canvas, XUI_BG_NORMAL, 0, rgb(25, 33, 44));
    for (row = 0; row < sizeof(grid) / sizeof(grid[0]); ++row) {
        for (col = 0; col < 3; ++col) {
            enum ButtonId id = grid[row][col];
            if (id != BUTTON_COUNT &&
                make_button(id, 16 + col * 154, KEYPAD_Y + row * ROW_STEP,
                            KEY_WIDTH, KEY_HEIGHT) < 0)
                goto cleanup;
        }
    }

    start_live();
    XuiClearKey();
    while (XuiIsRunning()) {
        if (!XuiHasKey()) {
            update_live();
            usleep(12000);
            continue;
        }
        key = XuiGetKey();
        if (key == XUI_KEYCANCEL || key == BUTTON_KEY + EXIT) break;
        switch (key) {
        case XUI_KEY1: key = BUTTON_KEY + MODE; break;
        case XUI_KEY1 + 1: key = BUTTON_KEY + LAYOUT; break;
        case XUI_KEY1 + 2: key = BUTTON_KEY + FACE; break;
        case XUI_KEY1 + 3: key = BUTTON_KEY + LIGHTER; break;
        case XUI_KEY1 + 4: key = BUTTON_KEY + CELL; break;
        case XUI_KEY1 + 5: key = BUTTON_KEY + DARKER; break;
        case XUI_KEY1 + 6: key = BUTTON_KEY + DENSITY; break;
        case XUI_KEY1 + 7: key = BUTTON_KEY + FEED; break;
        default: break;
        }
        if (key == XUI_KEYCLEAR || key == BUTTON_KEY + RETURN_LIVE) {
            start_live();
        } else if (key == XUI_KEYENTER || key == BUTTON_KEY + ACTION) {
            if (state == CAMERA_LIVE) snap();
            else if (state == PHOTO_REVIEW) {
                if (key == BUTTON_KEY + ACTION || millis() - action_ms >= ACTION_DELAY_MS)
                    print_photo();
                else
                    action_ms = millis();
            }
        } else if (key == BUTTON_KEY + MODE) {
            mode = mode == PIXEL_BOLD ? PIXEL_SHADE : mode + 1;
            settings_changed();
        } else if (key == BUTTON_KEY + CELL) {
            cell_size = cell_size == 4 ? 2 : cell_size + 1;
            if (mode == PIXEL_BOLD) draw_controls();
            else settings_changed();
        } else if (key == BUTTON_KEY + DENSITY) {
            print_density = print_density == 4 ? 1 : print_density == 1 ? 3 : 4;
            draw_controls();
        } else if (key == BUTTON_KEY + FEED) {
            feed_paper();
        } else if (key == BUTTON_KEY + LAYOUT) {
            layout = layout == PHOTO_WIDE ? PHOTO_TALL : PHOTO_WIDE;
            settings_changed();
        } else if (key == BUTTON_KEY + FACE) {
            face_enabled = !face_enabled;
            settings_changed();
        } else if (key == BUTTON_KEY + LIGHTER || key == BUTTON_KEY + DARKER) {
            int next = adjustment + (key == BUTTON_KEY + LIGHTER ? -ADJUST_STEP : ADJUST_STEP);
            if (next >= -ADJUST_LIMIT && next <= ADJUST_LIMIT) {
                adjustment = next;
                settings_changed();
            }
        }
    }
    result = 0;

cleanup:
    if (camera) XuiDestroyWindow(camera);
    for (i = 0; i < BUTTON_COUNT; ++i)
        if (buttons[i].window) XuiDestroyWindow(buttons[i].window);
    if (photo_canvas) XuiDestroyWindow(photo_canvas);
    if (live_canvas) XuiDestroyWindow(live_canvas);
    if (photo_image) XuiImgFree(photo_image);
    if (live_image) XuiImgFree(live_image);
    if (font) XuiDestroyFont(font);
    XuiClose();
    if (result) puts("Pixel Camera could not start");
    return result;
}
