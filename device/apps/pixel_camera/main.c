/* Camera snapshots converted to full-width monochrome thermal prints. */
#include <osal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xui.h>

#define PIXEL_PATH "/data/app/pixel_camera/latest_pixel.png"
#define CAMERA_640_480 1
#define PREVIEW_75 1
#define PRINT_WIDTH 384
#define PRINT_HEIGHT 288
#define CELL_SIZE 3
#define CELL_COLS (PRINT_WIDTH / CELL_SIZE)
#define CELL_ROWS (PRINT_HEIGHT / CELL_SIZE)
#define ROW_BYTES (PRINT_WIDTH / 8)
#define CHUNK_HEIGHT 144
#define PACKET_SIZE (1 + CHUNK_HEIGHT * (2 + ROW_BYTES))
#define SNAP_KEY 1001
#define PRINT_KEY 1002
#define RETAKE_KEY 1003
#define EXIT_KEY 1004
#define MODE_KEY 1005

enum PrintMode { BOLD_MODE, SHADE_MODE };

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static XuiWindow *screen;
static XuiWindow *camera;
static XuiWindow *pixel_canvas;
static XuiFont *font;
static XuiImg *preview_image;
static Button snap_button;
static Button print_button;
static Button retake_button;
static Button exit_button;
static Button mode_button;
static unsigned char bitmap[PRINT_HEIGHT * ROW_BYTES];
static unsigned char packet[PACKET_SIZE];
static unsigned char gray[PRINT_WIDTH * PRINT_HEIGHT];
static unsigned char cell_gray[CELL_COLS * CELL_ROWS];
static int bitmap_ready;
static enum PrintMode print_mode = BOLD_MODE;

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void status(const char *message)
{
    XuiCanvasDrawRect(screen, 20, 519, 440, 70, rgb(47, 55, 64), 1, 1);
    XuiCanvasDrawText(screen, 33, 535, 31, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)message);
}

static int make_button(Button *button, int x, int y, int width, const char *label,
                       int key, int background, int pressed_background,
                       int foreground)
{
    button->window = XuiCreateButton(screen, x, y, width, 72);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = foreground;
    button->normal.text_font = font;
    button->normal.text_x = 17;
    button->normal.text_y = 18;
    button->normal.text_height = 34;
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

static void mark_pixel(unsigned char *rgba, int x, int y, int black)
{
    int offset = (y * PRINT_WIDTH + x) * 4;
    unsigned char value = black ? 0 : 255;
    rgba[offset] = value;
    rgba[offset + 1] = value;
    rgba[offset + 2] = value;
    rgba[offset + 3] = 255;
    if (black)
        bitmap[y * ROW_BYTES + x / 8] |= (unsigned char)(0x80 >> (x & 7));
}

static int luminance_at(const unsigned char *source, int x, int y)
{
    int offset = (y * PRINT_WIDTH + x) * 4;
    return (77 * source[offset] + 150 * source[offset + 1] +
            29 * source[offset + 2]) / 256;
}

static int blurred_luminance(const unsigned char *source, int x, int y)
{
    int dx, dy, nx, ny, sum = 0, count = 0;
    for (dy = -1; dy <= 1; ++dy) {
        ny = y + dy;
        if (ny < 0 || ny >= PRINT_HEIGHT)
            continue;
        for (dx = -1; dx <= 1; ++dx) {
            nx = x + dx;
            if (nx < 0 || nx >= PRINT_WIDTH)
                continue;
            sum += luminance_at(source, nx, ny);
            ++count;
        }
    }
    return sum / count;
}

static int image_threshold(const unsigned int *histogram)
{
    int level, total = PRINT_WIDTH * PRINT_HEIGHT;
    int total_sum = 0, background_count = 0, background_sum = 0;
    int best_threshold = 128, best_score = -1;
    for (level = 0; level < 256; ++level)
        total_sum += level * (int)histogram[level];
    for (level = 0; level < 256; ++level) {
        int foreground_count, mean_background, mean_foreground;
        int difference, score;
        background_count += (int)histogram[level];
        background_sum += level * (int)histogram[level];
        foreground_count = total - background_count;
        if (background_count < 256 || foreground_count < 256)
            continue;
        mean_background = background_sum / background_count;
        mean_foreground = (total_sum - background_sum) / foreground_count;
        difference = mean_foreground - mean_background;
        score = (background_count / 256) * (foreground_count / 256) *
                (difference * difference / 256);
        if (score > best_score) {
            best_score = score;
            best_threshold = (mean_background + mean_foreground) / 2;
        }
    }
    return best_threshold;
}

static void prepare_gray(const unsigned char *source)
{
    int x, y;
    for (y = 0; y < PRINT_HEIGHT; ++y)
        for (x = 0; x < PRINT_WIDTH; ++x)
            gray[y * PRINT_WIDTH + x] =
                (unsigned char)blurred_luminance(source, x, y);
}

static void high_contrast(unsigned char *output)
{
    unsigned int histogram[256] = {0};
    int x, y, value, threshold;
    for (y = 0; y < PRINT_HEIGHT; ++y)
        for (x = 0; x < PRINT_WIDTH; ++x) {
            value = gray[y * PRINT_WIDTH + x];
            ++histogram[value];
        }
    threshold = image_threshold(histogram);
    memset(bitmap, 0, sizeof(bitmap));
    for (y = 0; y < PRINT_HEIGHT; ++y)
        for (x = 0; x < PRINT_WIDTH; ++x)
            mark_pixel(output, x, y,
                       gray[y * PRINT_WIDTH + x] < threshold);
}

static int cell_black_dots(int luminance)
{
    int dots;
    if (luminance > 255)
        luminance = 255;
    dots = ((255 - luminance) * 9 + 127) / 255;
    if (dots == 1)
        dots = 0;
    if (dots > 8)
        dots = 8;
    return dots;
}

static void shade_dots(unsigned char *output)
{
    /* The number of connected dots in each 3x3 cell represents gray. */
    static const unsigned char rank[3][3] = {
        {0, 4, 1},
        {6, 8, 5},
        {2, 7, 3},
    };
    int cx, cy, dx, dy, sum, lift, used_lift = 0, dots, total_dots;
    for (cy = 0; cy < CELL_ROWS; ++cy)
        for (cx = 0; cx < CELL_COLS; ++cx) {
            sum = 0;
            for (dy = 0; dy < CELL_SIZE; ++dy)
                for (dx = 0; dx < CELL_SIZE; ++dx)
                    sum += gray[(cy * CELL_SIZE + dy) * PRINT_WIDTH +
                                cx * CELL_SIZE + dx];
            cell_gray[cy * CELL_COLS + cx] = (unsigned char)(sum / 9);
        }
    for (lift = 0; lift <= 80; lift += 8) {
        total_dots = 0;
        used_lift = lift;
        for (cy = 0; cy < CELL_ROWS; ++cy)
            for (cx = 0; cx < CELL_COLS; ++cx)
                total_dots += cell_black_dots(
                    cell_gray[cy * CELL_COLS + cx] + lift);
        if (total_dots <= PRINT_WIDTH * PRINT_HEIGHT * 45 / 100)
            break;
    }
    memset(bitmap, 0, sizeof(bitmap));
    for (cy = 0; cy < CELL_ROWS; ++cy)
        for (cx = 0; cx < CELL_COLS; ++cx) {
            dots = cell_black_dots(cell_gray[cy * CELL_COLS + cx] + used_lift);
            for (dy = 0; dy < CELL_SIZE; ++dy)
                for (dx = 0; dx < CELL_SIZE; ++dx)
                    mark_pixel(output, cx * CELL_SIZE + dx,
                               cy * CELL_SIZE + dy,
                               rank[dy][dx] >= 9 - dots);
        }
    printf("Pixel shade lift=%d black=%d\n", used_lift, total_dots);
    fflush(stdout);
}

static int prepare_snapshot(void)
{
    XuiImg *captured = NULL;
    XuiImg *generated = NULL;
    XuiImg *loaded = NULL;
    unsigned char *source = NULL;
    unsigned char *output = NULL;
    int result = -1;

    captured = XuiCameraCapture(camera);
    if (!captured)
        goto cleanup;
    if (XuiImgResize(captured, PRINT_WIDTH, PRINT_HEIGHT) < 0)
        goto cleanup;
    source = malloc(PRINT_WIDTH * PRINT_HEIGHT * 4);
    output = malloc(PRINT_WIDTH * PRINT_HEIGHT * 4);
    if (!source || !output)
        goto cleanup;
    if (XuiImgToRgba(captured, (char *)source) < 0)
        goto cleanup;
    prepare_gray(source);
    if (print_mode == BOLD_MODE)
        high_contrast(output);
    else
        shade_dots(output);
    generated = XuiRgbaToImg((char *)output,
                             PRINT_WIDTH * PRINT_HEIGHT * 4,
                             PRINT_WIDTH, PRINT_HEIGHT);
    if (!generated || XuiImgSaveToFile(generated, PIXEL_PATH) < 0)
        goto cleanup;
    loaded = XuiImgLoadFromFile(PIXEL_PATH);
    if (!loaded || XuiImgResize(loaded, 480, 360) < 0)
        goto cleanup;
    XuiCanvasSetBackground(pixel_canvas, XUI_BG_NORMAL, 0, rgb(255, 255, 255));
    if (XuiCanvasDrawImg(pixel_canvas, 0, 0, 480, 360,
                         XUI_BG_NORMAL, loaded) < 0)
        goto cleanup;
    if (preview_image)
        XuiImgFree(preview_image);
    preview_image = loaded;
    loaded = NULL;
    result = 0;

cleanup:
    if (loaded)
        XuiImgFree(loaded);
    if (generated)
        XuiImgFree(generated);
    if (captured)
        XuiImgFree(captured);
    free(source);
    free(output);
    return result;
}

static void snap(void)
{
    if (!camera) {
        status("CAMERA NOT READY");
        return;
    }
    bitmap_ready = 0;
    status("MAKING PIXEL PHOTO");
    if (prepare_snapshot() < 0) {
        status("SNAP FAILED");
        puts("Pixel photo failed");
        fflush(stdout);
        return;
    }
    bitmap_ready = 1;
    XuiShowWindow(camera, 0, 0);
    XuiShowWindow(pixel_canvas, 1, 0);
    status(print_mode == BOLD_MODE ? "BOLD READY" : "SHADE READY");
    puts("Pixel photo ready");
    fflush(stdout);
}

static void queue_image_chunk(int first_row)
{
    int row, index = 0;
    packet[index++] = CHUNK_HEIGHT;
    for (row = 0; row < CHUNK_HEIGHT; ++row) {
        packet[index++] = 0;
        packet[index++] = ROW_BYTES;
        memcpy(packet + index, bitmap + (first_row + row) * ROW_BYTES,
               ROW_BYTES);
        index += ROW_BYTES;
    }
    OsPrnPutImage(packet);
}

static void print_photo(void)
{
    int opened, check, result;
    char message[64];
    if (!bitmap_ready) {
        status("SNAP A PHOTO FIRST");
        return;
    }
    opened = OsPrnOpen(PRN_REAL, NULL);
    if (opened != RET_OK) {
        snprintf(message, sizeof(message), "PRINTER OPEN: %d", opened);
        status(message);
        printf("Pixel printer open=%d\n", opened);
        fflush(stdout);
        return;
    }
    check = OsPrnCheck();
    if (check != RET_OK) {
        OsPrnClose();
        snprintf(message, sizeof(message), "PRINTER CHECK: %d", check);
        status(message);
        printf("Pixel printer check=%d\n", check);
        fflush(stdout);
        return;
    }
    status("PRINTING PHOTO");
    OsPrnClrBuf();
    OsPrnSetGray(4);
    OsPrnSetIndent(0, 0);
    OsPrnSetSpace(0, 0);
    queue_image_chunk(0);
    queue_image_chunk(CHUNK_HEIGHT);
    OsPrnFeed(48);
    result = OsPrnStart();
    OsPrnClose();
    snprintf(message, sizeof(message), "PRINT RESULT: %d", result);
    status(message);
    printf("Pixel print result=%d\n", result);
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
    XuiCanvasDrawText(screen, 44, 17, 50, font, XUI_BOLD,
                      rgb(255, 255, 255), "Q80 PIXEL CAMERA");
    XuiCanvasDrawText(screen, 44, 81, 24, font, XUI_BOLD,
                      rgb(194, 207, 212), "SNAP / PREVIEW / PRINT");
    pixel_canvas = XuiCreateCanvas(screen, 0, 132, 480, 360);
    if (!pixel_canvas)
        goto cleanup;
    XuiCanvasSetBackground(pixel_canvas, XUI_BG_NORMAL, 0, rgb(31, 38, 45));
    XuiShowWindow(pixel_canvas, 0, 0);
    if (make_button(&snap_button, 20, 614, 140, "SNAP", SNAP_KEY,
                    rgb(0, 201, 52), rgb(0, 153, 39),
                    rgb(255, 255, 255)) < 0 ||
        make_button(&print_button, 170, 614, 140, "PRINT", PRINT_KEY,
                    rgb(255, 255, 255), rgb(221, 229, 226),
                    rgb(16, 16, 16)) < 0 ||
        make_button(&mode_button, 320, 614, 140, "MODE", MODE_KEY,
                    rgb(55, 63, 72), rgb(80, 90, 101),
                    rgb(255, 255, 255)) < 0 ||
        make_button(&retake_button, 20, 704, 210, "RETAKE", RETAKE_KEY,
                    rgb(55, 63, 72), rgb(80, 90, 101),
                    rgb(255, 255, 255)) < 0 ||
        make_button(&exit_button, 250, 704, 210, "EXIT", EXIT_KEY,
                    rgb(255, 135, 0), rgb(205, 100, 0),
                    rgb(255, 255, 255)) < 0)
        goto cleanup;

    camera = XuiCreateCamera(screen, 0, 132, 0,
                             CAMERA_640_480, PREVIEW_75);
    if (camera) {
        XuiShowWindow(camera, 1, 0);
        status("BOLD MODE - TAP SNAP");
        puts("Pixel camera preview started");
    } else {
        status("CAMERA OPEN FAILED");
        puts("Pixel camera preview failed");
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
        if (key == SNAP_KEY)
            snap();
        else if (key == PRINT_KEY)
            print_photo();
        else if (key == RETAKE_KEY && camera) {
            bitmap_ready = 0;
            XuiShowWindow(pixel_canvas, 0, 0);
            XuiShowWindow(camera, 1, 0);
            status("LIVE - TAP SNAP");
        } else if (key == MODE_KEY && camera) {
            print_mode = print_mode == BOLD_MODE ? SHADE_MODE : BOLD_MODE;
            bitmap_ready = 0;
            XuiShowWindow(pixel_canvas, 0, 0);
            XuiShowWindow(camera, 1, 0);
            status(print_mode == BOLD_MODE ? "BOLD MODE - TAP SNAP" :
                                           "SHADE MODE - TAP SNAP");
        }
    }

cleanup:
    if (camera)
        XuiDestroyWindow(camera);
    if (exit_button.window)
        XuiDestroyWindow(exit_button.window);
    if (retake_button.window)
        XuiDestroyWindow(retake_button.window);
    if (mode_button.window)
        XuiDestroyWindow(mode_button.window);
    if (print_button.window)
        XuiDestroyWindow(print_button.window);
    if (snap_button.window)
        XuiDestroyWindow(snap_button.window);
    if (pixel_canvas)
        XuiDestroyWindow(pixel_canvas);
    if (preview_image)
        XuiImgFree(preview_image);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    if (result != 0)
        puts("Pixel Camera could not start");
    return result;
}
