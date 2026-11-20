#ifndef Q80_XUI_MANUAL_ONLY_H
#define Q80_XUI_MANUAL_ONLY_H

/* XUI Guide 2.1.7 functions absent from this Q80's libxui.so.
 * These declarations are for comparing API versions, not for calls on this Q80.
 */
#include "xui.h"

int XuiCanvasDrawTextEx(XuiWindow *window, unsigned int x, unsigned int y, unsigned int height,
                        XuiFont *font, XuiTextStyle textstyle, long fg, char *text, int linebreak);
int XuiCameraSetStat(XuiWindow *window, XuiCameraStat *stat);
int XuiImgToGray8(XuiImg *img, const char *gray8);
int XuiImgToBgr24(XuiImg *img, char *bgr24);
XuiImg *XuiFrameBufferToImg(char *data, unsigned int len, unsigned int width, unsigned int height);
XuiImg *XuiImgLoadFromBase64(const char *base64string, int *errcode);

#endif
