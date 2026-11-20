/* A small Q80 app: show text until a key is pressed. */
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <xui.h>

/* Derive function pointer types from xui.h, then load the device library at runtime. */
typedef __typeof__(&XuiOpen) xui_open_fn;
typedef __typeof__(&XuiClose) xui_close_fn;
typedef __typeof__(&XuiRootCanvas) xui_root_fn;
typedef __typeof__(&XuiCreateFont) xui_font_fn;
typedef __typeof__(&XuiDestroyFont) xui_destroy_font_fn;
typedef __typeof__(&XuiCanvasSetBackground) xui_background_fn;
typedef __typeof__(&XuiCanvasDrawText) xui_text_fn;
typedef __typeof__(&XuiIsRunning) xui_running_fn;
typedef __typeof__(&XuiHasKey) xui_has_key_fn;
typedef __typeof__(&XuiGetKey) xui_get_key_fn;
typedef __typeof__(&XuiClearKey) xui_clear_key_fn;

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

int app_main(int argc, char **argv)
{
    static const char *const dependencies[] = {
        "librt.so.1", "libpthread.so.0", "libgcc_s.so.1", "libfribidi.so.0",
        "/lib/libm.so.6", "libfreetype.so.6", "libpng12.so.0",
        "libts-1.0.so.0"
    };
    void *xui;
    XuiWindow *root;
    XuiFont *font;
    xui_open_fn open_xui;
    xui_close_fn close_xui;
    xui_root_fn root_canvas;
    xui_font_fn create_font;
    xui_destroy_font_fn destroy_font;
    xui_background_fn set_background;
    xui_text_fn draw_text;
    xui_running_fn is_running;
    xui_has_key_fn has_key;
    xui_get_key_fn get_key;
    xui_clear_key_fn clear_key;
    unsigned int i;

    (void)argc;
    (void)argv;

    /* The stock XUI library needs these libraries in the global scope first. */
    for (i = 0; i < sizeof(dependencies) / sizeof(dependencies[0]); ++i) {
        if (!dlopen(dependencies[i], RTLD_NOW | RTLD_GLOBAL)) {
            puts(dlerror());
            return 1;
        }
    }
    xui = dlopen("libxui.so", RTLD_NOW | RTLD_GLOBAL);
    if (!xui) {
        puts(dlerror());
        return 1;
    }

    open_xui = (xui_open_fn)dlsym(xui, "XuiOpen");
    close_xui = (xui_close_fn)dlsym(xui, "XuiClose");
    root_canvas = (xui_root_fn)dlsym(xui, "XuiRootCanvas");
    create_font = (xui_font_fn)dlsym(xui, "XuiCreateFont");
    destroy_font = (xui_destroy_font_fn)dlsym(xui, "XuiDestroyFont");
    set_background = (xui_background_fn)dlsym(xui, "XuiCanvasSetBackground");
    draw_text = (xui_text_fn)dlsym(xui, "XuiCanvasDrawText");
    is_running = (xui_running_fn)dlsym(xui, "XuiIsRunning");
    has_key = (xui_has_key_fn)dlsym(xui, "XuiHasKey");
    get_key = (xui_get_key_fn)dlsym(xui, "XuiGetKey");
    clear_key = (xui_clear_key_fn)dlsym(xui, "XuiClearKey");
    if (!open_xui || !close_xui || !root_canvas || !create_font ||
        !destroy_font || !set_background || !draw_text || !is_running ||
        !has_key || !get_key || !clear_key) {
        puts("XUI function missing");
        return 1;
    }

    if (open_xui(0, 0) < 0) {
        puts("XUI open failed");
        return 1;
    }
    root = root_canvas();
    font = create_font("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!root || !font) {
        puts("XUI canvas or font missing");
        if (font)
            destroy_font(font);
        close_xui();
        return 1;
    }

    set_background(root, XUI_BG_NORMAL, 0, rgb(207, 219, 216));
    draw_text(root, 40, 150, 46, font, XUI_BOLD,
              rgb(22, 43, 45), "HELLO Q80");
    draw_text(root, 40, 225, 30, font, XUI_BOLD,
              rgb(26, 105, 89), "Screen demo");
    draw_text(root, 40, 305, 30, font, XUI_BOLD,
              rgb(47, 69, 70), "PRESS ANY KEY TO EXIT");
    clear_key();
    while (is_running()) {
        if (has_key()) {
            get_key();
            break;
        }
        usleep(20000);
    }

    destroy_font(font);
    close_xui();
    return 0;
}
