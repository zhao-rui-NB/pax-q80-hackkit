#ifndef PIXEL_RASTER_H
#define PIXEL_RASTER_H

enum PixelMode { PIXEL_SHADE, PIXEL_CLUSTER, PIXEL_BOLD };
enum PhotoLayout { PHOTO_WIDE, PHOTO_TALL };

void pixel_lift_center(const unsigned char *source, int width, int height,
                       unsigned char *destination);

void pixel_convert(const unsigned char *source, int source_width, int source_height,
                   int width, int height, enum PhotoLayout layout,
                   enum PixelMode mode, int adjustment, int cell_size,
                   unsigned char *gray, unsigned char *bitmap);

#endif
