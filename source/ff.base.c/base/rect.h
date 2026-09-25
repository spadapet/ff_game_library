#pragma once

#include "../base/math.h"
#include "../base/point.h"

typedef struct ff_rect_float
{
    float left;
    float top;
    float right;
    float bottom;
} ff_rect_float;

typedef struct ff_rect_int
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} ff_rect_int;

typedef struct ff_rect_size
{
    size_t left;
    size_t top;
    size_t right;
    size_t bottom;
} ff_rect_size;

static inline ff_rect_float ff_rect_float_make(float left, float top, float right, float bottom)
{
    ff_rect_float value;
    value.left = left;
    value.top = top;
    value.right = right;
    value.bottom = bottom;
    return value;
}

static inline ff_rect_int ff_rect_int_make(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    ff_rect_int value;
    value.left = left;
    value.top = top;
    value.right = right;
    value.bottom = bottom;
    return value;
}

static inline ff_rect_size ff_rect_size_make(size_t left, size_t top, size_t right, size_t bottom)
{
    ff_rect_size value;
    value.left = left;
    value.top = top;
    value.right = right;
    value.bottom = bottom;
    return value;
}

static inline ff_rect_float ff_rect_float_zero(void)
{
    return ff_rect_float_make(0.0f, 0.0f, 0.0f, 0.0f);
}

static inline ff_rect_int ff_rect_int_zero(void)
{
    return ff_rect_int_make(0, 0, 0, 0);
}

static inline ff_rect_float ff_rect_float_from_corners(ff_point_float top_left, ff_point_float bottom_right)
{
    return ff_rect_float_make(top_left.x, top_left.y, bottom_right.x, bottom_right.y);
}

static inline ff_rect_float ff_rect_float_from_size(ff_point_float top_left, ff_point_float size)
{
    return ff_rect_float_make(top_left.x, top_left.y, top_left.x + size.x, top_left.y + size.y);
}

static inline bool ff_rect_float_equal(ff_rect_float l, ff_rect_float r)
{
    return l.left == r.left && l.top == r.top && l.right == r.right && l.bottom == r.bottom;
}

static inline bool ff_rect_int_equal(ff_rect_int l, ff_rect_int r)
{
    return l.left == r.left && l.top == r.top && l.right == r.right && l.bottom == r.bottom;
}

static inline float ff_rect_float_width(ff_rect_float value)
{
    return value.right - value.left;
}

static inline float ff_rect_float_height(ff_rect_float value)
{
    return value.bottom - value.top;
}

static inline int32_t ff_rect_int_width(ff_rect_int value)
{
    return value.right - value.left;
}

static inline int32_t ff_rect_int_height(ff_rect_int value)
{
    return value.bottom - value.top;
}

static inline ff_point_float ff_rect_float_size(ff_rect_float value)
{
    return ff_point_float_make(ff_rect_float_width(value), ff_rect_float_height(value));
}

static inline ff_point_float ff_rect_float_top_left(ff_rect_float value)
{
    return ff_point_float_make(value.left, value.top);
}

static inline ff_point_float ff_rect_float_bottom_right(ff_rect_float value)
{
    return ff_point_float_make(value.right, value.bottom);
}

static inline ff_point_float ff_rect_float_center(ff_rect_float value)
{
    return ff_point_float_make((value.left + value.right) / 2.0f, (value.top + value.bottom) / 2.0f);
}

static inline float ff_rect_float_area(ff_rect_float value)
{
    return ff_rect_float_width(value) * ff_rect_float_height(value);
}

static inline bool ff_rect_float_empty(ff_rect_float value)
{
    return value.left >= value.right || value.top >= value.bottom;
}

static inline bool ff_rect_int_empty(ff_rect_int value)
{
    return value.left >= value.right || value.top >= value.bottom;
}

static inline bool ff_rect_float_contains(ff_rect_float value, ff_point_float point)
{
    return point.x >= value.left && point.x < value.right && point.y >= value.top && point.y < value.bottom;
}

static inline bool ff_rect_float_intersects(ff_rect_float l, ff_rect_float r)
{
    return l.left < r.right && l.right > r.left && l.top < r.bottom && l.bottom > r.top;
}

// Empty when the inputs don't overlap, since the clamped edges can cross.
static inline ff_rect_float ff_rect_float_intersection(ff_rect_float l, ff_rect_float r)
{
    return ff_rect_float_make(
        ff_math_max_float(l.left, r.left),
        ff_math_max_float(l.top, r.top),
        ff_math_min_float(l.right, r.right),
        ff_math_min_float(l.bottom, r.bottom));
}

static inline ff_rect_float ff_rect_float_boundary(ff_rect_float l, ff_rect_float r)
{
    return ff_rect_float_make(
        ff_math_min_float(l.left, r.left),
        ff_math_min_float(l.top, r.top),
        ff_math_max_float(l.right, r.right),
        ff_math_max_float(l.bottom, r.bottom));
}

static inline ff_rect_float ff_rect_float_normalize(ff_rect_float value)
{
    return ff_rect_float_make(
        ff_math_min_float(value.left, value.right),
        ff_math_min_float(value.top, value.bottom),
        ff_math_max_float(value.left, value.right),
        ff_math_max_float(value.top, value.bottom));
}

static inline ff_rect_float ff_rect_float_offset(ff_rect_float value, float x, float y)
{
    return ff_rect_float_make(value.left + x, value.top + y, value.right + x, value.bottom + y);
}

static inline ff_rect_float ff_rect_float_move_top_left(ff_rect_float value, ff_point_float top_left)
{
    return ff_rect_float_from_size(top_left, ff_rect_float_size(value));
}

static inline ff_rect_float ff_rect_float_inflate(ff_rect_float value, float x, float y)
{
    return ff_rect_float_make(value.left - x, value.top - y, value.right + x, value.bottom + y);
}

static inline ff_rect_float ff_rect_float_deflate(ff_rect_float value, float x, float y)
{
    return ff_rect_float_inflate(value, -x, -y);
}

static inline ff_rect_float ff_rect_float_scale(ff_rect_float value, ff_point_float scale)
{
    return ff_rect_float_make(value.left * scale.x, value.top * scale.y, value.right * scale.x, value.bottom * scale.y);
}

static inline ff_rect_float ff_rect_int_to_float(ff_rect_int value)
{
    return ff_rect_float_make((float)value.left, (float)value.top, (float)value.right, (float)value.bottom);
}

static inline ff_rect_float ff_rect_size_to_float(ff_rect_size value)
{
    return ff_rect_float_make((float)value.left, (float)value.top, (float)value.right, (float)value.bottom);
}

static inline ff_rect_int ff_rect_float_to_int(ff_rect_float value)
{
    return ff_rect_int_make((int32_t)value.left, (int32_t)value.top, (int32_t)value.right, (int32_t)value.bottom);
}
