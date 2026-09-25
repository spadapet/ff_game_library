#pragma once

typedef struct ff_point_float
{
    float x;
    float y;
} ff_point_float;

typedef struct ff_point_int
{
    int32_t x;
    int32_t y;
} ff_point_int;

typedef struct ff_point_size
{
    size_t x;
    size_t y;
} ff_point_size;

static inline ff_point_float ff_point_float_make(float x, float y)
{
    ff_point_float value;
    value.x = x;
    value.y = y;
    return value;
}

static inline ff_point_int ff_point_int_make(int32_t x, int32_t y)
{
    ff_point_int value;
    value.x = x;
    value.y = y;
    return value;
}

static inline ff_point_size ff_point_size_make(size_t x, size_t y)
{
    ff_point_size value;
    value.x = x;
    value.y = y;
    return value;
}

static inline ff_point_float ff_point_float_zero(void)
{
    return ff_point_float_make(0.0f, 0.0f);
}

static inline ff_point_int ff_point_int_zero(void)
{
    return ff_point_int_make(0, 0);
}

static inline bool ff_point_float_equal(ff_point_float l, ff_point_float r)
{
    return l.x == r.x && l.y == r.y;
}

static inline bool ff_point_int_equal(ff_point_int l, ff_point_int r)
{
    return l.x == r.x && l.y == r.y;
}

static inline ff_point_float ff_point_float_add(ff_point_float l, ff_point_float r)
{
    return ff_point_float_make(l.x + r.x, l.y + r.y);
}

static inline ff_point_float ff_point_float_subtract(ff_point_float l, ff_point_float r)
{
    return ff_point_float_make(l.x - r.x, l.y - r.y);
}

static inline ff_point_float ff_point_float_multiply(ff_point_float l, ff_point_float r)
{
    return ff_point_float_make(l.x * r.x, l.y * r.y);
}

static inline ff_point_float ff_point_float_scale(ff_point_float value, float scale)
{
    return ff_point_float_make(value.x * scale, value.y * scale);
}

static inline ff_point_int ff_point_int_add(ff_point_int l, ff_point_int r)
{
    return ff_point_int_make(l.x + r.x, l.y + r.y);
}

static inline ff_point_int ff_point_int_subtract(ff_point_int l, ff_point_int r)
{
    return ff_point_int_make(l.x - r.x, l.y - r.y);
}

static inline ff_point_float ff_point_int_to_float(ff_point_int value)
{
    return ff_point_float_make((float)value.x, (float)value.y);
}

static inline ff_point_float ff_point_size_to_float(ff_point_size value)
{
    return ff_point_float_make((float)value.x, (float)value.y);
}

static inline ff_point_int ff_point_float_to_int(ff_point_float value)
{
    return ff_point_int_make((int32_t)value.x, (int32_t)value.y);
}
