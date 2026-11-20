/* Directly linked OSAL and XUI example. */
#include <osal.h>
#include <unistd.h>
#include <xui.h>

static int rgb(int red, int green, int blue)
{
    return (int)XuiColor(blue, green, red, 255);
}

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

    XuiCanvasSetBackground(canvas, XUI_BG_NORMAL, 0, rgb(207, 219, 216));
    XuiCanvasDrawRect(canvas, 24, 42, width - 48, 8, rgb(26, 105, 89), 0, 1);
    XuiCanvasDrawText(canvas, 36, 110, 88, font, XUI_BOLD,
                      rgb(22, 43, 45), "Q80");
    XuiCanvasDrawText(canvas, 36, 255, 36, font, XUI_BOLD,
                      rgb(26, 105, 89), "Q80 中文字顯示測試");
    XuiCanvasDrawText(canvas, 36, height - 160, 30, font, XUI_BOLD,
                      rgb(47, 69, 70), "PRESS ANY KEY TO EXIT");
    XuiClearKey();
    while (XuiIsRunning()) {
        if (XuiHasKey()) {
            XuiGetKey();
            break;
        }
        usleep(20000);
    }

    XuiDestroyFont(font);
    XuiClose();
    return 0;
}
