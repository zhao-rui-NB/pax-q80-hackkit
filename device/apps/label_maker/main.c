/* Q80 label editor: fixed touch keyboard, libchewing composition, no print on startup. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <osal.h>
#include <xui.h>
#include "ime.h"
#include "keycaps.h"

#define LINE_BYTES 512
#define LABEL_WIDTH 384
#define LABEL_HEIGHT 240
#define LABEL_ROW_BYTES (LABEL_WIDTH / 8)
#define MIN_LABEL_SIZE 14
#define LABEL_LINE_GAP 10
#define LABEL_ROWS ((LABEL_HEIGHT + LABEL_LINE_GAP) / (MIN_LABEL_SIZE + LABEL_LINE_GAP))
#define WORKSPACE_X 20
#define WORKSPACE_WIDTH 440
#define WORKSPACE_Y 112
#define WORKSPACE_HEIGHT 548
#define PREVIEW_X (WORKSPACE_X + (WORKSPACE_WIDTH - LABEL_WIDTH) / 2)
#define PREVIEW_Y (WORKSPACE_Y + (WORKSPACE_HEIGHT - LABEL_HEIGHT) / 2)
#define EDIT_TEXT_Y 65
#define EDIT_HEIGHT 140
#define EDIT_FONT_SIZE 40
#define KEY_FIRST 300
#define CAND_FIRST 400
#define KEY_EXIT 513
#define KEY_PRINT 514
#define KEY_SMALLER 515
#define KEY_LARGER 516
#define KEY_NEXT 520
#define KEY_MODE 521
#define KEY_SPACE 522
#define KEY_BACK 523
#define KEY_DONE 524
#define KEY_CANCEL 525
#define KEY_LEFT 526
#define KEY_RIGHT 527
#define KEY_NEWLINE 528
#define KEY_UP 529
#define KEY_DOWN 530
#define GESTURE_UP 13

/* DaChen layout, in exactly the same order as keycaps.h. */
static const char *zhuyin_glyphs[] = {
    "ㄅ","ㄉ","ˇ","ˋ","ㄓ","ˊ","˙","ㄚ","ㄞ","ㄢ","ㄦ",
    "ㄆ","ㄊ","ㄍ","ㄐ","ㄔ","ㄗ","ㄧ","ㄛ","ㄟ","ㄣ",
    "ㄇ","ㄋ","ㄎ","ㄑ","ㄕ","ㄘ","ㄨ","ㄜ","ㄠ","ㄤ",
    "ㄈ","ㄌ","ㄏ","ㄒ","ㄖ","ㄙ","ㄩ","ㄝ","ㄡ","ㄥ"
};
static const char *zhuyin_keys[4] = {
    "1234567890-", "qwertyuiop", "asdfghjkl;", "zxcvbnm,./"
};
static const char *digits[4] = {
    "1234567890-", "@#$%&*()[]", "!?.,:;'+=/", "_~^<>\\{}|`"
};
static XuiWindow *screen, *main_canvas, *editor_canvas, *ui_canvas;
static XuiWindow *preview;
static XuiFont *font, *chinese_font;
static XuiImg *glyphs[41];
typedef struct {
    XuiWindow *window;
    XuiButtonStat normal, pressed;
} Button;
static Button print_button, exit_button;
static Button smaller_button, larger_button, left_button, right_button;
static Button newline_button;
static Button up_button, down_button;
static Button keys[41], candidates[5], next_button;
static Button mode_button, space_button, back_button, done_button, cancel_button;
static char line[LINE_BYTES], draft[LINE_BYTES];
static char candidate_text[5][48], ascii_text[41][4];
static char mode_text[16];
static int editing, mode, candidate_page;
static int received_touch;
static int preview_fits;
static int label_size = 30;
static size_t cursor;
static size_t edit_start, edit_end;
static int composition_width;
static size_t upper_row_start, lower_row_start;

static int draft_width(size_t start, size_t end, int size)
{
    char text[LINE_BYTES];
    if (start == end) return 0;
    memcpy(text, draft + start, end - start);
    text[end - start] = 0;
    return XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, text);
}

static int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}

static size_t next_char(const char *text, size_t at)
{
    if (!text[at]) return at;
    do { ++at; } while (((unsigned char)text[at] & 0xc0) == 0x80);
    return at;
}

static size_t previous_char(const char *text, size_t at)
{
    while (at && ((unsigned char)text[--at] & 0xc0) == 0x80) {}
    return at;
}

static int rgb(int r, int g, int b) { return (int)XuiColor(b, g, r, 255); }
static void destroy(Button *button)
{
    if (button->window) XuiDestroyWindow(button->window);
    button->window = 0;
}
static int button(Button *b, int x, int y, int w, int h, char *label,
                  int key, int bg, int fg, int size)
{
    b->window = XuiCreateButton(ui_canvas, x, y, w, h);
    if (!b->window) return -1;
    b->normal = (XuiButtonStat){0};
    b->normal.btn_round = 1;
    b->normal.btn_bg = bg;
    b->normal.text = label;
    b->normal.text_fg = fg;
    b->normal.text_font = font;
    b->normal.text_x = 8;
    b->normal.text_y = (h - size) / 2;
    b->normal.text_height = size;
    b->pressed = b->normal;
    b->pressed.btn_bg = rgb(205, 219, 224);
    if (XuiButtonSetStat(b->window, XUI_BTN_NORMAL, &b->normal) < 0 ||
        XuiButtonSetStat(b->window, XUI_BTN_PRESSED, &b->pressed) < 0 ||
        XuiButtonSetKey(b->window, key) < 0) return -1;
    XuiShowWindow(b->window, 1, 0);
    return 0;
}
static void update(Button *b)
{
    XuiButtonSetStat(b->window, XUI_BTN_NORMAL, &b->normal);
    XuiButtonSetStat(b->window, XUI_BTN_PRESSED, &b->pressed);
}
static int load_glyphs(void)
{
    int i, pixel;
    unsigned char rgba[KEYCAP_SIZE * KEYCAP_SIZE * 4];
    for (i = 0; i < 41; ++i) {
        for (pixel = 0; pixel < KEYCAP_SIZE * KEYCAP_SIZE; ++pixel) {
            int black = (keycaps[i][pixel / 8] >> (7 - pixel % 8)) & 1;
            rgba[pixel * 4] = black ? 0 : 255;
            rgba[pixel * 4 + 1] = black ? 0 : 255;
            rgba[pixel * 4 + 2] = black ? 0 : 255;
            rgba[pixel * 4 + 3] = 255;
        }
        glyphs[i] = XuiRgbaToImg((char *)rgba, sizeof(rgba), KEYCAP_SIZE, KEYCAP_SIZE);
        if (!glyphs[i]) return -1;
    }
    return 0;
}
static void heading(const char *title, const char *subtitle)
{
    XuiCanvasSetBackground(ui_canvas, XUI_BG_NORMAL, 0, rgb(8, 11, 15));
    XuiCanvasDrawRect(ui_canvas, 20, 19, 8, 73, rgb(0, 201, 52), 0, 1);
    XuiCanvasDrawText(ui_canvas, 42, 17, 42, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)title);
    XuiCanvasDrawText(ui_canvas, 42, 72, 25, font, XUI_BOLD,
                      rgb(190, 207, 212), (char *)subtitle);
}
static void message(const char *text)
{
    XuiCanvasDrawRect(ui_canvas, 20, 667, 440, 34, rgb(47, 55, 64), 1, 1);
    XuiCanvasDrawText(ui_canvas, 32, 672, 22, font, XUI_BOLD,
                      rgb(255, 255, 255), (char *)text);
}
static void destroy_main(void)
{
    if (main_canvas) XuiShowWindow(main_canvas, 0, 0);
    destroy(&exit_button);
    destroy(&print_button);
    destroy(&smaller_button);
    destroy(&larger_button);
    if (preview) XuiDestroyWindow(preview);
    preview = 0;
    if (main_canvas) XuiDestroyWindow(main_canvas);
    main_canvas = 0;
}

static int draw_label(void)
{
    char rows[LABEL_ROWS][LINE_BYTES] = {{0}};
    const char *start = line;
    const char *end = line;
    int row = 0, i, y, widths[LABEL_ROWS];
    int spacing = label_size + LABEL_LINE_GAP;

    XuiCanvasDrawRect(preview, 0, 0, LABEL_WIDTH, LABEL_HEIGHT,
                      rgb(255,255,255), 0, 1);
    while (*end) {
        const char *next = line + next_char(line, (size_t)(end - line));
        if (*end == '\n') {
            if (++row == LABEL_ROWS) return 0;
            start = end = next;
            continue;
        }
        memcpy(rows[row], start, (size_t)(next - start));
        rows[row][next - start] = 0;
        if (XuiTextWidthEx(chinese_font, label_size, XUI_TEXT_NORMAL, rows[row]) > LABEL_WIDTH - 32) {
            rows[row][end - start] = 0;
            if (++row == LABEL_ROWS || end == start) return 0;
            start = end;
        } else {
            end = next;
        }
    }
    if (!line[0]) return 1;
    if (row * spacing + label_size > LABEL_HEIGHT) return 0;
    for (i = 0; i <= row; ++i) {
        widths[i] = rows[i][0] ?
            XuiTextWidthEx(chinese_font, label_size, XUI_TEXT_NORMAL, rows[i]) : 0;
    }
    y = (LABEL_HEIGHT - row * spacing - label_size) / 2;
    for (i = 0; i <= row; ++i) {
        if (!rows[i][0]) continue;
        XuiCanvasDrawText(preview, (LABEL_WIDTH - widths[i]) / 2,
                          y + i * spacing, label_size,
                          chinese_font, XUI_TEXT_NORMAL, rgb(0,0,0), rows[i]);
    }
    return 1;
}

static void refresh_preview(void)
{
    char status[48];
    preview_fits = draw_label();
    snprintf(status, sizeof(status), "TEXT SIZE %d", label_size);
    message(!line[0] ? "ENTER TEXT TO START" :
            preview_fits ? status : "REDUCE SIZE OR SHORTEN TEXT");
}

static int is_tap(const XuiGesture *gesture)
{
    return gesture->type == GESTURE_UP &&
           abs(gesture->cur_x - gesture->down_x) <= 8 &&
           abs(gesture->cur_y - gesture->down_y) <= 8;
}

static void print_label(void)
{
    static unsigned char packet[1 + LABEL_HEIGHT * (2 + LABEL_ROW_BYTES)];
    unsigned char *rgba;
    XuiImg *image;
    int row, col, result, black = 0, index = 1;

    if (!line[strspn(line, " \n")]) { message("ENTER TEXT FIRST"); return; }
    if (!preview_fits) { message("TEXT IS TOO LONG"); return; }
    image = XuiCaptureCanvas(preview, 0, 0, LABEL_WIDTH, LABEL_HEIGHT);
    if (!image) { message("PREVIEW CAPTURE FAILED"); return; }
    rgba = malloc(LABEL_WIDTH * LABEL_HEIGHT * 4);
    result = rgba ? XuiImgToRgba(image, (char *)rgba) : -1;
    XuiImgFree(image);
    if (result < 0) { free(rgba); message("PREVIEW CAPTURE FAILED"); return; }

    packet[0] = LABEL_HEIGHT;
    for (row = 0; row < LABEL_HEIGHT; ++row) {
        packet[index++] = 0;
        packet[index++] = LABEL_ROW_BYTES;
        for (col = 0; col < LABEL_WIDTH; ++col) {
            const unsigned char *pixel = rgba + (row * LABEL_WIDTH + col) * 4;
            if (col % 8 == 0) packet[index] = 0;
            if (pixel[0] + pixel[1] + pixel[2] < 384) {
                packet[index] |= 0x80 >> (col % 8);
                ++black;
            }
            if (col % 8 == 7) ++index;
        }
    }
    free(rgba);
    if (!black) { message("LABEL IS BLANK"); return; }

    result = OsPrnOpen(PRN_REAL, NULL);
    if (result == 0) {
        result = OsPrnCheck();
        if (result == 0) {
            OsPrnClrBuf();
            OsPrnSetGray(4);
            OsPrnSetIndent(0, 0);
            OsPrnSetSpace(0, 0);
            OsPrnPutImage(packet);
            OsPrnFeed(48);
            result = OsPrnStart();
        }
        OsPrnClose();
    }
    if (result == 0) {
        message("LABEL PRINTED");
    } else {
        char status[48];
        snprintf(status, sizeof(status), "PRINT FAILED (%d)", result);
        message(status);
    }
    XuiClearKey();
}

static int draw_main(void)
{
    if (!main_canvas) {
        main_canvas = XuiCreateCanvas(screen, 0, 0, 480, 800);
        if (!main_canvas) return -1;
        XuiShowWindow(main_canvas, 0, 0);
        ui_canvas = main_canvas;
        heading("Q80 LABEL MAKER", "TAP THE LABEL TO EDIT");
        XuiCanvasDrawRect(main_canvas, WORKSPACE_X, WORKSPACE_Y,
                          WORKSPACE_WIDTH, WORKSPACE_HEIGHT, rgb(255,255,255), 0, 1);
        preview = XuiCreateCanvas(main_canvas, PREVIEW_X, PREVIEW_Y, LABEL_WIDTH, LABEL_HEIGHT);
        if (!preview) return -1;
        XuiShowWindow(preview, 1, 0);
        if (button(&smaller_button, 20, 715, 96, 65, "A-", KEY_SMALLER,
                   rgb(255,255,255), rgb(16,16,16), 27) < 0 ||
            button(&larger_button, 126, 715, 96, 65, "A+", KEY_LARGER,
                   rgb(255,255,255), rgb(16,16,16), 27) < 0 ||
            button(&print_button, 232, 715, 124, 65, "PRINT", KEY_PRINT,
                   rgb(0,201,52), rgb(16,16,16), 25) < 0 ||
            button(&exit_button, 366, 715, 94, 65, "EXIT", KEY_EXIT,
                   rgb(255,135,0), rgb(255,255,255), 25) < 0) return -1;
    }
    ui_canvas = main_canvas;
    refresh_preview();
    XuiSetGestureRect(WORKSPACE_X, WORKSPACE_Y, WORKSPACE_WIDTH, WORKSPACE_HEIGHT);
    XuiClearGesture();
    XuiClearKey();
    XuiShowWindow(main_canvas, 1, 0);
    return 0;
}
static void destroy_editor(void)
{
    int i;
    /* Hide the whole keyboard at once, then release its children off screen. */
    if (editor_canvas) XuiShowWindow(editor_canvas, 0, 0);
    destroy(&cancel_button); destroy(&done_button); destroy(&back_button);
    destroy(&space_button); destroy(&mode_button);
    destroy(&left_button); destroy(&right_button);
    destroy(&newline_button);
    destroy(&up_button); destroy(&down_button);
    for (i = 40; i >= 0; --i) destroy(&keys[i]);
    destroy(&next_button);
    for (i = 4; i >= 0; --i) destroy(&candidates[i]);
    if (editor_canvas) XuiDestroyWindow(editor_canvas);
    editor_canvas = 0;
    ui_canvas = screen;
}
static void update_key_mode(void)
{
    int i, row, col;
    for (i = 0; i < 41; ++i) {
        row = i < 11 ? 0 : i < 21 ? 1 : i < 31 ? 2 : 3;
        col = i - (row == 0 ? 0 : row == 1 ? 11 : row == 2 ? 21 : 31);
        ascii_text[i][0] = (mode == 2 ? digits[row][col] : zhuyin_keys[row][col]);
        ascii_text[i][1] = 0;
        keys[i].normal.img = mode == 0 ? glyphs[i] : 0;
        keys[i].normal.img_x = 2;
        keys[i].normal.img_y = 13;
        keys[i].normal.img_style = XUI_BG_NORMAL;
        keys[i].normal.text = ascii_text[i];
        keys[i].normal.text_height = mode == 0 ? 17 : 31;
        keys[i].normal.text_y = mode == 0 ? 48 : 18;
        keys[i].pressed = keys[i].normal;
        keys[i].pressed.btn_bg = rgb(205,219,224);
        update(&keys[i]);
    }
    snprintf(mode_text, sizeof(mode_text), "%s", mode == 0 ? "ZH" : mode == 1 ? "ABC" : "123");
    mode_button.normal.text = mode_text;
    mode_button.pressed.text = mode_text;
    update(&mode_button);
}
static void append(const char *text)
{
    size_t used = strlen(draft), count = strlen(text);
    if (!count || used + count >= sizeof(draft)) return;
    memmove(draft + cursor + count, draft + cursor, used - cursor + 1);
    memcpy(draft + cursor, text, count);
    cursor += count;
}
static void collect_commit(void)
{
    append(ime_commit());
}
static void finish_composition(void)
{
    if (!ime_buffer()[0] && !ime_bopomofo()[0]) return;
    if (ime_enter() == 0) collect_commit();
}
static void backspace(void)
{
    if (mode == 0 && (ime_buffer()[0] || ime_bopomofo()[0])) {
        ime_backspace(); collect_commit();
    } else if (cursor) {
        size_t start = previous_char(draft, cursor);
        memmove(draft + start, draft + cursor, strlen(draft + cursor) + 1);
        cursor = start;
    }
}
static void refresh_editor(void)
{
    char before[LINE_BYTES], active[LINE_BYTES * 2], after[LINE_BYTES];
    char status[48];
    const char *phonetic = mode == 0 ? ime_bopomofo() : "";
    int i, count = mode == 0 ? ime_candidate_count() : 0, size = EDIT_FONT_SIZE;
    int x, y = EDIT_TEXT_Y + 12, width;
    size_t start = 0, end = 0, line_start, line_end;
    size_t starts[LINE_BYTES], ends[LINE_BYTES];
    unsigned row = 0, selected = 0;
    starts[0] = 0;
    while (draft[end]) {
        size_t next = next_char(draft, end);
        if (draft[end] == '\n') {
            ends[row++] = end;
            starts[row] = next;
        } else if (end > starts[row] &&
                   draft_width(starts[row], next, label_size) > LABEL_WIDTH - 32) {
            ends[row++] = end;
            starts[row] = end;
        }
        end = next;
    }
    ends[row] = end;
    while (selected < row && starts[selected + 1] <= cursor) ++selected;
    upper_row_start = selected > 0 ? starts[selected - 1] : cursor;
    lower_row_start = selected < row ? starts[selected + 1] : cursor;
    line_start = starts[selected];
    line_end = ends[selected];
    memcpy(before, draft + line_start, cursor - line_start);
    before[cursor - line_start] = 0;
    snprintf(active, sizeof(active), "%s", mode == 0 ? ime_buffer() : "");
    XuiCanvasDrawRect(ui_canvas, 20, EDIT_TEXT_Y, 440, EDIT_HEIGHT,
                      rgb(255,255,255), 0, 1);
    while (before[start] &&
           XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, before + start) >
               (active[0] ? 310 : 410))
        start = next_char(before, start);
    edit_start = line_start + start;
    width = before[start] ? XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, before + start) : 0;
    XuiCanvasDrawText(ui_canvas, 30, y + 2, size, chinese_font, XUI_TEXT_NORMAL,
                      rgb(16,16,16), before + start);
    x = 30 + width;
    while (active[0] &&
           x + XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, active) >
                          444) {
        size_t skip = next_char(active, 0);
        memmove(active, active + skip, strlen(active + skip) + 1);
    }
    if (active[0]) {
        XuiCanvasDrawText(ui_canvas, x, y + 2, size, chinese_font, XUI_TEXT_NORMAL,
                          rgb(0,105,180), active);
        x += XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, active);
    }
    composition_width = x - 30 - width;
    XuiCanvasDrawRect(ui_canvas, x, y + 2, 2, 42, rgb(0,105,180), 0, 1);
    x += 2;
    end = line_end - cursor;
    memcpy(after, draft + cursor, end);
    after[end] = 0;
    while (end && x + XuiTextWidthEx(chinese_font, size, XUI_TEXT_NORMAL, after) > 450) {
        end = previous_char(after, end);
        after[end] = 0;
    }
    edit_end = cursor + end;
    if (after[0])
        XuiCanvasDrawText(ui_canvas, x, y + 2, size, chinese_font, XUI_TEXT_NORMAL,
                          rgb(16,16,16), after);
    x = 30;
    while (*phonetic && x <= 396) {
        for (i = 0; i < 41; ++i) {
            size_t n = strlen(zhuyin_glyphs[i]);
            if (strncmp(phonetic, zhuyin_glyphs[i], n) == 0) {
                XuiCanvasDrawImg(ui_canvas, x, y + 56, 48, 48, XUI_BG_NORMAL, glyphs[i]);
                x += 52; phonetic += n;
                break;
            }
        }
        if (i == 41) ++phonetic;
    }
    if (x > 30)
        XuiCanvasDrawRect(ui_canvas, x, y + 57, 2, 46, rgb(0,105,180), 0, 1);
    XuiCanvasDrawRect(ui_canvas, 20, 270, 440, 38, rgb(47,55,64), 1, 1);
    snprintf(status, sizeof(status), "%s   LINE %u / %u",
             mode == 0 ? "ZHUYIN" : mode == 1 ? "ENGLISH" : "NUMBERS",
             selected + 1, row + 1);
    XuiCanvasDrawText(ui_canvas, 28, 275, 25, font, XUI_BOLD,
                      rgb(255,255,255), status);
    if (candidate_page * 5 >= count) candidate_page = 0;
    for (i = 0; i < 5; ++i) {
        const char *text = candidate_page * 5 + i < count
                           ? ime_candidate(candidate_page * 5 + i) : "";
        snprintf(candidate_text[i], sizeof(candidate_text[i]), "%s", text);
        candidates[i].normal.text = candidate_text[i];
        candidates[i].pressed.text = candidate_text[i];
        candidates[i].normal.text_font = chinese_font;
        candidates[i].pressed.text_font = chinese_font;
        update(&candidates[i]);
    }
}
static void tap_editor(int x, int y)
{
    size_t start, end, target, old_cursor;
    int position, previous_width = 0;
    if (x < 20 || x >= 460 || y < EDIT_TEXT_Y ||
        y >= EDIT_TEXT_Y + 64 || ime_bopomofo()[0])
        return;
    start = edit_start;
    end = edit_end;
    position = x - 30;
    {
        int before = draft_width(start, cursor, EDIT_FONT_SIZE);
        if (position > before) {
            position -= composition_width + 2;
            if (position < before) position = before;
        }
    }
    for (target = start; target < end;) {
        size_t next = next_char(draft, target);
        int width = draft_width(start, next, EDIT_FONT_SIZE);
        if (position <= (previous_width + width) / 2) break;
        target = next;
        previous_width = width;
    }
    old_cursor = cursor;
    finish_composition();
    if (target >= old_cursor) target += cursor - old_cursor;
    cursor = target;
    candidate_page = 0;
    refresh_editor();
}
static int draw_editor(void)
{
    int i, row, col, x, y, width;
    snprintf(draft, sizeof(draft), "%s", line);
    cursor = strlen(draft);
    XuiSetGestureRect(20, EDIT_TEXT_Y, 440, EDIT_HEIGHT);
    XuiClearGesture();
    mode = 0; candidate_page = 0; received_touch = 0;
    if (ime_open() < 0) return -1;
    if (editor_canvas) {
        ui_canvas = editor_canvas;
        update_key_mode();
        refresh_editor();
        XuiClearKey();
        editing = 1;
        XuiShowWindow(editor_canvas, 1, 0);
        return 0;
    }
    editor_canvas = XuiCreateCanvas(screen, 0, 0, 480, 800);
    if (!editor_canvas) return -1;
    XuiShowWindow(editor_canvas, 0, 0);
    ui_canvas = editor_canvas;
    XuiCanvasSetBackground(ui_canvas, XUI_BG_NORMAL, 0, rgb(8,11,15));
    XuiCanvasDrawText(ui_canvas, 20, 18, 25, font, XUI_BOLD,
                      rgb(255,255,255), "USE UP OR DOWN TO CHANGE LINE");
    if (button(&left_button, 20, 214, 65, 48, "<", KEY_LEFT,
               rgb(255,255,255), rgb(16,16,16), 26) < 0 ||
        button(&right_button, 93, 214, 65, 48, ">", KEY_RIGHT,
               rgb(255,255,255), rgb(16,16,16), 26) < 0 ||
        button(&up_button, 166, 214, 65, 48, "UP", KEY_UP,
               rgb(255,255,255), rgb(16,16,16), 23) < 0 ||
        button(&down_button, 239, 214, 90, 48, "DOWN", KEY_DOWN,
               rgb(255,255,255), rgb(16,16,16), 23) < 0 ||
        button(&newline_button, 337, 214, 123, 48, "NEW LINE", KEY_NEWLINE,
               rgb(255,255,255), rgb(16,16,16), 21) < 0)
        return -1;
    for (i = 0; i < 5; ++i)
        if (button(&candidates[i], 20 + i * 70, 318, 66, 53,
                   candidate_text[i], CAND_FIRST + i, rgb(255,255,255),
                   rgb(16,16,16), 27) < 0) return -1;
    if (button(&next_button, 375, 318, 85, 53, "NEXT", KEY_NEXT,
               rgb(255,135,0), rgb(255,255,255), 24) < 0) return -1;
    for (i = 0; i < 41; ++i) {
        row = i < 11 ? 0 : i < 21 ? 1 : i < 31 ? 2 : 3;
        col = i - (row == 0 ? 0 : row == 1 ? 11 : row == 2 ? 21 : 31);
        width = row == 0 ? 40 : 43;
        x = (row == 0 ? 9 : 7) + col * (row == 0 ? 42 : 46);
        y = 390 + row * 75;
        if (button(&keys[i], x, y, width, 68, ascii_text[i],
                   KEY_FIRST + i, rgb(255,255,255), rgb(16,16,16), 29) < 0) return -1;
    }
    if (button(&mode_button, 10, 702, 85, 81, mode_text, KEY_MODE,
               rgb(255,135,0), rgb(255,255,255), 27) < 0 ||
        button(&space_button, 100, 702, 120, 81, "SPACE", KEY_SPACE,
               rgb(255,255,255), rgb(16,16,16), 27) < 0 ||
        button(&back_button, 225, 702, 90, 81, "DEL", KEY_BACK,
               rgb(255,255,255), rgb(16,16,16), 27) < 0 ||
        button(&done_button, 320, 702, 80, 81, "DONE", KEY_DONE,
               rgb(0,201,52), rgb(255,255,255), 24) < 0 ||
        button(&cancel_button, 405, 702, 70, 81, "X", KEY_CANCEL,
               rgb(55,63,72), rgb(255,255,255), 27) < 0) return -1;
    update_key_mode();
    refresh_editor();
    XuiClearKey();
    editing = 1;
    XuiShowWindow(editor_canvas, 1, 0);
    puts("Label editor ready"); fflush(stdout);
    return 0;
}
static void finish_editor(int save)
{
    if (save) {
        finish_composition();
        snprintf(line, sizeof(line), "%s", draft);
    }
    XuiShowWindow(editor_canvas, 0, 0);
    editing = 0;
    ui_canvas = main_canvas;
    if (draw_main() < 0) puts("Label main screen failed");
    ime_close();
}
static void editor_key(int key)
{
    int index;
    if (!received_touch) {
        puts("Label editor received input"); fflush(stdout);
        received_touch = 1;
    }
    if (key == KEY_CANCEL || key == XUI_KEYCANCEL) { finish_editor(0); return; }
    if (key == KEY_DONE || key == XUI_KEYENTER) { finish_editor(1); return; }
    if (key >= KEY_FIRST && key < KEY_FIRST + 41) {
        index = key - KEY_FIRST;
        if (mode == 0) { ime_key(zhuyin_keys[index < 11 ? 0 : index < 21 ? 1 : index < 31 ? 2 : 3]
                              [index < 11 ? index : index < 21 ? index - 11 : index < 31 ? index - 21 : index - 31]);
                         collect_commit(); }
        else append(ascii_text[index]);
        candidate_page = 0;
    } else if (key >= CAND_FIRST && key < CAND_FIRST + 5) {
        index = candidate_page * 5 + key - CAND_FIRST;
        if (mode == 0 && index < ime_candidate_count()) {
            ime_choose(index); collect_commit(); candidate_page = 0;
        }
    } else if (key == KEY_NEXT) ++candidate_page;
    else if (key == KEY_LEFT || key == KEY_RIGHT) {
        if (!ime_bopomofo()[0]) {
            finish_composition();
            cursor = key == KEY_LEFT ? previous_char(draft, cursor) : next_char(draft, cursor);
            candidate_page = 0;
        }
    } else if (key == KEY_UP || key == KEY_DOWN) {
        if (!ime_bopomofo()[0]) {
            size_t target = key == KEY_UP ? upper_row_start : lower_row_start;
            size_t old_cursor = cursor;
            finish_composition();
            cursor = target + (target >= old_cursor ? cursor - old_cursor : 0);
            candidate_page = 0;
        }
    } else if (key == KEY_NEWLINE) {
        if (!ime_bopomofo()[0]) {
            finish_composition();
            append("\n");
            candidate_page = 0;
        }
    }
    else if (key == KEY_BACK || key == XUI_KEYCLEAR) backspace();
    else if (key == KEY_SPACE) {
        if (mode == 0) { ime_space(); collect_commit(); }
        else append(" ");
        candidate_page = 0;
    } else if (key == KEY_MODE) {
        if (mode == 0) finish_composition();
        mode = (mode + 1) % 3; candidate_page = 0;
        update_key_mode();
    }
    refresh_editor();
}

int app_main(int argc, char **argv)
{
    int key, result = 1, i;
    if (XuiOpen(argc, argv) < 0) return 1;
    screen = XuiRootCanvas();
    ui_canvas = screen;
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    chinese_font = XuiCreateFont("/data/app/label_maker/NotoSansTC.ttf", 0, XUI_FONT_GREY);
    if (!screen || !font || !chinese_font || load_glyphs() < 0) goto cleanup;
    if (draw_main() < 0) goto cleanup;
    while (XuiIsRunning()) {
        XuiGesture gesture;
        int has_gesture = XuiGetGesture(&gesture) > 0;
        if (has_gesture && editing && is_tap(&gesture)) {
            tap_editor(gesture.cur_x, gesture.cur_y);
            continue;
        }
        if (has_gesture && !editing && is_tap(&gesture)) {
            if (gesture.down_x < WORKSPACE_X || gesture.down_x >= WORKSPACE_X + WORKSPACE_WIDTH ||
                gesture.down_y < WORKSPACE_Y || gesture.down_y >= WORKSPACE_Y + WORKSPACE_HEIGHT)
                continue;
            XuiShowWindow(main_canvas, 0, 0);
            if (draw_editor() < 0) {
                destroy_editor(); ime_close(); editing = 0;
                draw_main(); message("KEYBOARD UNAVAILABLE");
            }
            continue;
        }
        if (!XuiHasKey()) { usleep(12000); continue; }
        key = XuiGetKey();
        if (editing) { editor_key(key); continue; }
        if (key == KEY_EXIT || key == XUI_KEYCANCEL) { result = 0; break; }
        if (key == KEY_PRINT) { print_label(); continue; }
        if (key == KEY_SMALLER || key == KEY_LARGER) {
            label_size = clamp(label_size + (key == KEY_SMALLER ? -4 : 4), MIN_LABEL_SIZE, 54);
            refresh_preview();
            continue;
        }
    }
cleanup:
    destroy_editor(); destroy_main(); ime_close();
    for (i = 0; i < 41; ++i) if (glyphs[i]) XuiImgFree(glyphs[i]);
    if (chinese_font) XuiDestroyFont(chinese_font);
    if (font) XuiDestroyFont(font);
    XuiClose();
    return result;
}
