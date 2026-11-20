/* A small Q80 app: show text with the device's XUI library for ten seconds. */
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
    if (!open_xui || !close_xui || !root_canvas || !create_font ||
        !destroy_font || !set_background || !draw_text) {
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

    set_background(root, XUI_BG_NORMAL, 0, XuiColor(32, 32, 32, 255));
    draw_text(root, 40, 150, 40, font, XUI_TEXT_NORMAL,
              XuiColor(255, 255, 255, 255), "HELLO Q80");
    draw_text(root, 40, 220, 24, font, XUI_TEXT_NORMAL,
              XuiColor(255, 255, 255, 255), "Screen demo");
    sleep(10);

    destroy_font(font);
    close_xui();
    return 0;
}
