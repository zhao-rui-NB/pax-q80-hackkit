/* Draw on the native signature board and print the drawing on Q80 paper. */
#include <osal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xui.h>

#define BOARD_X 20
#define BOARD_Y 136
#define BOARD_SIZE 440
#define PRINT_SIZE 384
#define ROW_BYTES (PRINT_SIZE / 8)
#define CHUNK_ROWS (PRINT_SIZE / 2)
#define PACKET_SIZE (1 + CHUNK_ROWS * (2 + ROW_BYTES))
#define CLEAR_KEY 1001
#define PRINT_KEY 1002
#define EXIT_KEY 1003

typedef struct {
    XuiWindow *window;
    XuiButtonStat normal;
    XuiButtonStat pressed;
} Button;

static XuiWindow *screen;
static XuiWindow *board;
static XuiFont *font;
static XuiSigBoardStat board_state;
static Button clear_button;
static Button print_button;
static Button exit_button;
static unsigned char bitmap[PRINT_SIZE * ROW_BYTES];
static unsigned char packet[PACKET_SIZE];

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static void status(const char *message)
{
    XuiCanvasDrawRect(screen, 20, 599, 440, 61, rgb(47, 55, 64), 1, 1);
    XuiCanvasDrawText(screen, 34, 614, 30, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)message);
}

static int create_board(void)
{
    board = XuiCreateSignatureBoard(screen, BOARD_X, BOARD_Y,
                                    BOARD_SIZE, BOARD_SIZE);
    if (!board)
        return -1;
    board_state = (XuiSigBoardStat){0};
    board_state.btn_bg = rgb(255, 255, 255);
    board_state.pen_fg = rgb(16, 16, 16);
    board_state.pen_width = 10;
    board_state.pen_flat = 1;
    if (XuiSigBoardSetStat(board, &board_state) < 0)
        return -1;
    XuiShowWindow(board, 1, 0);
    return 0;
}

static int make_button(Button *button, int x, const char *label, int key,
                       int background, int foreground, int pressed_background)
{
    button->window = XuiCreateButton(screen, x, 688, 140, 76);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = foreground;
    button->normal.text_font = font;
    button->normal.text_x = 12;
    button->normal.text_y = 19;
    button->normal.text_height = 33;
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

static int prepare_bitmap(void)
{
    XuiImg *image;
    unsigned char *rgba;
    int row, col, offset, brightness, dark_pixels = 0;
    image = XuiSigBoardImg(board);
    if (!image)
        return -1;
    if (XuiImgResize(image, PRINT_SIZE, PRINT_SIZE) < 0) {
        XuiImgFree(image);
        return -1;
    }
    rgba = malloc(PRINT_SIZE * PRINT_SIZE * 4);
    if (!rgba) {
        XuiImgFree(image);
        return -1;
    }
    if (XuiImgToRgba(image, (char *)rgba) < 0) {
        free(rgba);
        XuiImgFree(image);
        return -1;
    }
    XuiImgFree(image);
    memset(bitmap, 0, sizeof(bitmap));
    for (row = 0; row < PRINT_SIZE; ++row)
        for (col = 0; col < PRINT_SIZE; ++col) {
            offset = (row * PRINT_SIZE + col) * 4;
            brightness = rgba[offset] + rgba[offset + 1] +
                         rgba[offset + 2];
            if (rgba[offset + 3] >= 128 && brightness < 450) {
                bitmap[row * ROW_BYTES + col / 8] |=
                    (unsigned char)(0x80 >> (col & 7));
                ++dark_pixels;
            }
        }
    free(rgba);
    printf("Sketch pixels=%d\n", dark_pixels);
    fflush(stdout);
    if (dark_pixels < 24)
        return 1;
    if (dark_pixels > PRINT_SIZE * PRINT_SIZE * 3 / 4)
        return -1;
    return 0;
}

static void queue_chunk(int first_row)
{
    int row, index = 0;
    packet[index++] = CHUNK_ROWS;
    for (row = 0; row < CHUNK_ROWS; ++row) {
        packet[index++] = 0;
        packet[index++] = ROW_BYTES;
        memcpy(packet + index,
               bitmap + (first_row + row) * ROW_BYTES, ROW_BYTES);
        index += ROW_BYTES;
    }
    OsPrnPutImage(packet);
}

static void print_drawing(void)
{
    int prepared, opened, checked, result;
    char message[64];
    status("PREPARING DRAWING");
    prepared = prepare_bitmap();
    if (prepared == 1) {
        status("DRAW SOMETHING FIRST");
        return;
    }
    if (prepared < 0) {
        status("DRAWING CAPTURE FAILED");
        puts("Sketch capture failed");
        fflush(stdout);
        return;
    }
    opened = OsPrnOpen(PRN_REAL, NULL);
    if (opened != RET_OK) {
        snprintf(message, sizeof(message), "PRINTER OPEN: %d", opened);
        status(message);
        printf("Sketch printer open=%d\n", opened);
        fflush(stdout);
        return;
    }
    checked = OsPrnCheck();
    if (checked != RET_OK) {
        OsPrnClose();
        snprintf(message, sizeof(message), "PRINTER CHECK: %d", checked);
        status(message);
        printf("Sketch printer check=%d\n", checked);
        fflush(stdout);
        return;
    }
    status("PRINTING DRAWING");
    OsPrnClrBuf();
    OsPrnSetGray(4);
    OsPrnSetIndent(0, 0);
    OsPrnSetSpace(0, 0);
    queue_chunk(0);
    queue_chunk(CHUNK_ROWS);
    OsPrnFeed(48);
    result = OsPrnStart();
    OsPrnClose();
    snprintf(message, sizeof(message), "PRINT RESULT: %d", result);
    status(message);
    printf("Sketch print result=%d\n", result);
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
    XuiCanvasDrawText(screen, 44, 17, 47, font, XUI_BOLD,
                      rgb(255, 255, 255), "Q80 SKETCH & PRINT");
    XuiCanvasDrawText(screen, 44, 81, 24, font, XUI_BOLD,
                      rgb(194, 207, 212), "DRAW WITH YOUR FINGER");
    XuiCanvasDrawRect(screen, BOARD_X - 3, BOARD_Y - 3,
                      BOARD_SIZE + 6, BOARD_SIZE + 6,
                      rgb(97, 109, 120), 0, 1);
    if (create_board() < 0)
        goto cleanup;
    if (make_button(&clear_button, 20, "CLEAR", CLEAR_KEY,
                    rgb(255, 135, 0), rgb(255, 255, 255),
                    rgb(206, 104, 0)) < 0 ||
        make_button(&print_button, 170, "PRINT", PRINT_KEY,
                    rgb(0, 201, 52), rgb(255, 255, 255),
                    rgb(0, 153, 39)) < 0 ||
        make_button(&exit_button, 320, "EXIT", EXIT_KEY,
                    rgb(55, 63, 72), rgb(255, 255, 255),
                    rgb(80, 90, 101)) < 0)
        goto cleanup;
    status("DRAW BEFORE PRINTING");

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
        if (key == XUI_KEYCLEAR || key == CLEAR_KEY) {
            XuiDestroyWindow(board);
            board = NULL;
            if (create_board() < 0)
                break;
            status("CANVAS CLEARED");
            XuiClearKey();
        } else if (key == PRINT_KEY) {
            print_drawing();
        }
    }

cleanup:
    if (exit_button.window)
        XuiDestroyWindow(exit_button.window);
    if (print_button.window)
        XuiDestroyWindow(print_button.window);
    if (clear_button.window)
        XuiDestroyWindow(clear_button.window);
    if (board)
        XuiDestroyWindow(board);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    if (result != 0)
        puts("Sketch & Print could not start");
    return result;
}
