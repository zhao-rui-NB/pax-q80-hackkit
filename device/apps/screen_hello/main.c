/* A small Q80 app: show text with the device's XUI library for ten seconds. */
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

typedef int (*xui_open_fn)(int, char **);
typedef void (*xui_close_fn)(void);
typedef void *(*xui_root_fn)(void);
typedef void *(*xui_font_fn)(char *, int, int);
typedef void (*xui_destroy_font_fn)(void *);
typedef void (*xui_background_fn)(void *, int, void *, unsigned long);
typedef int (*xui_text_fn)(void *, unsigned int, unsigned int, unsigned int,
                           void *, int, unsigned long, char *);

int app_main(int argc, char **argv)
{
    static const char *const dependencies[] = {
        "librt.so.1", "libpthread.so.0", "libgcc_s.so.1", "libfribidi.so.0",
        "/lib/libm.so.6", "libfreetype.so.6", "libpng12.so.0",
        "libts-1.0.so.0"
    };
    void *xui, *root, *font;
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
    font = create_font("/usr/font/tm_font", 0, 1);
    if (!root || !font) {
        puts("XUI canvas or font missing");
        if (font)
            destroy_font(font);
        close_xui();
        return 1;
    }

    set_background(root, 0, 0, 0xff202020UL);
    draw_text(root, 40, 150, 40, font, 0, 0xffffffffUL, "HELLO Q80");
    draw_text(root, 40, 220, 24, font, 0, 0xffffffffUL, "Screen demo");
    sleep(10);

    destroy_font(font);
    close_xui();
    return 0;
}
