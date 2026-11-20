/* Directly linked OSAL and XUI example. */
#include <osal.h>
#include <unistd.h>
#include <xui.h>

int app_main(int argc, char **argv)
{
    int width = 320;
    int height = 480;
    XuiWindow *canvas;
    XuiFont *font;

    OsScrGetSize(&width, &height);
    if (XuiOpen(argc, argv) < 0)
        return 1;

    canvas = XuiRootCanvas();
    font = XuiCreateFont("/usr/font/tm_font", 0, XUI_FONT_GREY);
    if (!canvas || !font) {
        if (font)
            XuiDestroyFont(font);
        XuiClose();
        return 1;
    }

    XuiCanvasSetBackground(canvas, XUI_BG_NORMAL, 0, XuiColor(20, 30, 50, 255));
    XuiCanvasDrawText(canvas, width / 5, height / 7, height / 5, font,
                      XUI_TEXT_NORMAL, XuiColor(255, 255, 255, 255), "Q80");
    XuiCanvasDrawText(canvas, width / 20, height * 3 / 5, height / 7, font,
                      XUI_TEXT_NORMAL, XuiColor(255, 255, 255, 255), "LINKED");
    sleep(60);

    XuiDestroyFont(font);
    XuiClose();
    return 0;
}
