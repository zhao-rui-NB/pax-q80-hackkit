/* Finger drawing on the Q80's native XUI signature board. */
#include <stdio.h>
#include <unistd.h>
#include <xui.h>

#define WIDTH 480
#define BOARD_X 20
#define BOARD_Y 128
#define BOARD_WIDTH 440
#define BOARD_HEIGHT 530
#define CLEAR_KEY 1001
#define EXIT_KEY 1002

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
static Button exit_button;

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

static int create_board(void)
{
    board = XuiCreateSignatureBoard(screen, BOARD_X, BOARD_Y,
                                    BOARD_WIDTH, BOARD_HEIGHT);
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

static int make_button(Button *button, int x, int y, int width,
                       const char *label, int key, int background,
                       int foreground, int pressed_background)
{
    button->window = XuiCreateButton(screen, x, y, width, 76);
    if (!button->window)
        return -1;
    button->normal = (XuiButtonStat){0};
    button->normal.btn_round = 1;
    button->normal.btn_bg = background;
    button->normal.text = (char *)label;
    button->normal.text_fg = foreground;
    button->normal.text_font = font;
    button->normal.text_x = 18;
    button->normal.text_y = 19;
    button->normal.text_height = 36;
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
                      rgb(255, 255, 255), "Q80 SKETCHPAD");
    XuiCanvasDrawText(screen, 44, 81, 24, font, XUI_BOLD,
                      rgb(194, 207, 212), "DRAW WITH YOUR FINGER");
    XuiCanvasDrawRect(screen, BOARD_X - 3, BOARD_Y - 3,
                      BOARD_WIDTH + 6, BOARD_HEIGHT + 6,
                      rgb(97, 109, 120), 0, 1);
    if (create_board() < 0)
        goto cleanup;
    if (make_button(&clear_button, 20, 688, 210, "CLEAR", CLEAR_KEY,
                    rgb(255, 135, 0), rgb(255, 255, 255),
                    rgb(206, 104, 0)) < 0 ||
        make_button(&exit_button, 250, 688, 210, "EXIT", EXIT_KEY,
                    rgb(55, 63, 72), rgb(255, 255, 255),
                    rgb(80, 90, 101)) < 0)
        goto cleanup;
    XuiCanvasDrawText(screen, 20, 771, 20, font, XUI_TEXT_NORMAL,
                      rgb(194, 207, 212), "DELETE KEY CLEARS THE CANVAS");

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
            XuiClearKey();
        }
    }

cleanup:
    if (clear_button.window)
        XuiDestroyWindow(clear_button.window);
    if (exit_button.window)
        XuiDestroyWindow(exit_button.window);
    if (board)
        XuiDestroyWindow(board);
    if (font)
        XuiDestroyFont(font);
    XuiClose();
    if (result != 0)
        printf("Sketchpad could not start\n");
    return result;
}
