#include "raster.h"

#include <string.h>

static int clamp_byte(int value)
{
    return value < 0 ? 0 : value > 255 ? 255 : value;
}

static int center_weight(int position, int length)
{
    int distance = position < length / 2 ? position : length - 1 - position;
    int weight = distance * 768 / length;
    return weight > 256 ? 256 : weight;
}

void pixel_lift_center(const unsigned char *source, int width, int height,
                       unsigned char *destination)
{
    int x, y;
    for (y = 0; y < height; ++y) {
        int vertical = center_weight(y, height);
        for (x = 0; x < width; ++x) {
            int light = source[y * width + x];
            int lift = light < 96 ? light * 2 / 3 : (255 - light) * 64 / 159;
            int weight = center_weight(x, width) * vertical;
            destination[y * width + x] = (unsigned char)(light + lift * weight / 65536);
        }
    }
}

/* Average every source pixel covered by the output pixel, including fractions. */
static int area_sample(const unsigned char *source, int sw, int sh, int x, int y,
                       int width, int height)
{
    int left = x * sw, right = left + sw, top = y * sh, bottom = top + sh;
    int first_x = left / width, last_x = (right - 1) / width;
    int first_y = top / height, last_y = (bottom - 1) / height;
    int sx, sy, sum = 0;
    for (sy = first_y; sy <= last_y; ++sy) {
        int upper = sy == first_y ? top : sy * height;
        int lower = sy == last_y ? bottom : (sy + 1) * height;
        for (sx = first_x; sx <= last_x; ++sx) {
            int start = sx == first_x ? left : sx * width;
            int end = sx == last_x ? right : (sx + 1) * width;
            sum += source[sy * sw + sx] * (end - start) * (lower - upper);
        }
    }
    return (sum + sw * sh / 2) / (sw * sh);
}

static void set_black(unsigned char *bitmap, int width, int x, int y)
{
    bitmap[y * (width / 8) + x / 8] |= (unsigned char)(0x80 >> (x % 8));
}

static int auto_threshold(const unsigned int *histogram, int total)
{
    int level, total_sum = 0, lower_count = 0, lower_sum = 0;
    int best_threshold = 128;
    int best = -1;
    for (level = 0; level < 256; ++level)
        total_sum += level * (int)histogram[level];
    for (level = 0; level < 255; ++level) {
        int upper_count, lower_mean, upper_mean, difference;
        int score;
        lower_count += (int)histogram[level];
        lower_sum += level * (int)histogram[level];
        upper_count = total - lower_count;
        if (lower_count < total / 500 || upper_count < total / 500 ||
            !lower_count || !upper_count)
            continue;
        lower_mean = lower_sum / lower_count;
        upper_mean = (total_sum - lower_sum) / upper_count;
        difference = upper_mean - lower_mean;
        score = (lower_count / 16) * (upper_count / 16) *
                (difference * difference / 256);
        if (score > best) {
            best = score;
            best_threshold = (lower_mean + upper_mean + 1) / 2;
        }
    }
    return best_threshold;
}

static void bold(const unsigned char *gray, int width, int height, int adjustment,
                 unsigned char *bitmap)
{
    unsigned int histogram[256] = {0};
    int x, y, threshold;
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) ++histogram[gray[y * width + x]];
    threshold = clamp_byte(auto_threshold(histogram, width * height) + adjustment);
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            if (gray[y * width + x] < threshold) set_black(bitmap, width, x, y);
}

static int cell_luminance(const unsigned char *gray, int width, int height,
                          int left, int top, int cell_size)
{
    int x, y, sum = 0, count = 0;
    for (y = top; y < top + cell_size && y < height; ++y)
        for (x = left; x < left + cell_size && x < width; ++x) {
            sum += gray[y * width + x];
            ++count;
        }
    return sum / count;
}

static int black_dots(int light, int cell_area)
{
    int dots = ((255 - clamp_byte(light)) * cell_area + 127) / 255;
    if (dots == 1 && cell_area > 4) return 0;
    return dots >= cell_area ? cell_area - 1 : dots;
}

static void halftone(unsigned char *gray, int width, int height,
                     enum PixelMode mode, int adjustment, int cell_size,
                     unsigned char *bitmap)
{
    static const unsigned char shade[][16] = {
        {0, 2, 3, 1},
        {8, 4, 7, 2, 0, 3, 6, 1, 5},
        {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5},
    };
    static const unsigned char cluster[][16] = {
        {0, 1, 2, 3},
        {5, 1, 6, 3, 0, 2, 7, 4, 8},
        {12, 6, 8, 13, 4, 0, 1, 9, 5, 2, 3, 10, 14, 7, 11, 15},
    };
    const unsigned char *rank = mode == PIXEL_CLUSTER ? cluster[cell_size - 2]
                                                     : shade[cell_size - 2];
    int cell_area = cell_size * cell_size;
    int left, top, x, y, lift, level;
    int histogram[256] = {0};

    for (top = 0; top < height; top += cell_size) {
        for (left = 0; left < width; left += cell_size) {
            int light = cell_luminance(gray, width, height, left, top, cell_size);
            gray[top * width + left] = (unsigned char)light;
            ++histogram[light];
        }
    }
    for (lift = 0; lift < 80; lift += 8) {
        int dots = 0;
        for (level = 0; level < 256; ++level)
            dots += histogram[level] * black_dots(level + lift, cell_area);
        if (dots <= width * height * 45 / 100) break;
    }

    for (top = 0; top < height; top += cell_size) {
        for (left = 0; left < width; left += cell_size) {
            int dots = black_dots(gray[top * width + left] + lift - adjustment, cell_area);
            for (y = top; y < top + cell_size && y < height; ++y)
                for (x = left; x < left + cell_size && x < width; ++x)
                    if (rank[(y - top) * cell_size + x - left] < dots) set_black(bitmap, width, x, y);
        }
    }
}

void pixel_convert(const unsigned char *source, int source_width, int source_height,
                   int width, int height, enum PhotoLayout layout,
                   enum PixelMode mode, int adjustment, int cell_size,
                   unsigned char *gray, unsigned char *bitmap)
{
    int x, y;
    memset(bitmap, 0, (size_t)(width / 8) * height);
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            int light = layout == PHOTO_TALL
                ? area_sample(source, source_width, source_height,
                              y, width - 1 - x, height, width)
                : area_sample(source, source_width, source_height, x, y, width, height);
            gray[y * width + x] = (unsigned char)light;
        }
    }
    if (mode == PIXEL_BOLD) bold(gray, width, height, adjustment, bitmap);
    else halftone(gray, width, height, mode, adjustment, cell_size, bitmap);
}
