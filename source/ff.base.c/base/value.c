#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/value.h"

static_assert(sizeof(ff_value) == 24, "ff_value must stay 24 bytes");

ff_value ff_value_new_empty(void)
{
    return (ff_value) { .type = ff_value_type_empty };
}

ff_value ff_value_new_null(void)
{
    return (ff_value) { .type = ff_value_type_null };
}

ff_value ff_value_new_boolean(bool value)
{
    return (ff_value)
    {
        .type = ff_value_type_boolean,
        .b = value,
    };
}

ff_value ff_value_new_int32(int32_t value)
{
    return (ff_value)
    {
        .type = ff_value_type_int32,
        .i32 = value,
    };
}

ff_value ff_value_new_int64(int64_t value)
{
    return (ff_value)
    {
        .type = ff_value_type_int64,
        .i64 = value,
    };
}

ff_value ff_value_new_float32(float value)
{
    return (ff_value)
    {
        .type = ff_value_type_float32,
        .f32 = value,
    };
}

ff_value ff_value_new_float64(double value)
{
    return (ff_value)
    {
        .type = ff_value_type_float64,
        .f64 = value,
    };
}

ff_value ff_value_new_point_int32(int32_t x, int32_t y)
{
    return (ff_value)
    {
        .type = ff_value_type_point_int32,
        .point_i32 = { x, y },
    };
}

ff_value ff_value_new_point_int64(int64_t x, int64_t y)
{
    return (ff_value)
    {
        .type = ff_value_type_point_int64,
        .point_i64 = { x, y },
    };
}

ff_value ff_value_new_point_float32(float x, float y)
{
    return (ff_value)
    {
        .type = ff_value_type_point_float32,
        .point_f32 = { x, y },
    };
}

ff_value ff_value_new_point_float64(double x, double y)
{
    return (ff_value)
    {
        .type = ff_value_type_point_float64,
        .point_f64 = { x, y },
    };
}

ff_value ff_value_new_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    return (ff_value)
    {
        .type = ff_value_type_rect_int32,
        .rect_i32 = { left, top, right, bottom },
    };
}

ff_value ff_value_new_rect_float32(float left, float top, float right, float bottom)
{
    return (ff_value)
    {
        .type = ff_value_type_rect_float32,
        .rect_f32 = { left, top, right, bottom },
    };
}

ff_value ff_value_new_guid(GUID value)
{
    return (ff_value)
    {
        .type = ff_value_type_guid,
        .guid = value,
    };
}

ff_value ff_value_new_dict(ff_dict* value)
{
    return (ff_value)
    {
        .type = ff_value_type_dict,
        .data = { .data = value, .count = 0, .item_size = 0, .item_align = 0 },
    };
}

ff_value ff_value_new_data(struct ff_span value)
{
    FF_ASSERT(value.size <= UINT32_MAX);

    ff_array_span span;
    span.data = value.data;
    span.count = (uint32_t)value.size;
    span.item_size = 1;
    span.item_align = alignof(size_t);

    return ff_value_new_data_array(span);
}

ff_value ff_value_new_data_array(struct ff_array_span value)
{
    return (ff_value)
    {
        .data = value,
        .type = ff_value_type_data,
    };
}

ff_value ff_value_new_string(ff_string_view value)
{
    ff_span span;
    span.data = value.data;
    span.size = value.count;

    ff_value result = ff_value_new_data(span);
    result.type = ff_value_type_string;
    return result;
}

ff_value ff_value_new_array(ff_value_span value)
{
    FF_ASSERT(value.count <= UINT32_MAX);

    ff_array_span span;
    span.data = value.data;
    span.count = (uint32_t)value.count;
    span.item_size = sizeof(ff_value);
    span.item_align = alignof(ff_value);

    ff_value result = ff_value_new_data_array(span);
    result.type = ff_value_type_array;
    return result;
}

ff_dict* ff_value_as_dict(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_dict);
    return (ff_dict*)value->data.data;
}

struct ff_span ff_value_as_data(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_data || value->type == ff_value_type_string || value->type == ff_value_type_array);
    return (ff_span)
    {
        .data = value->data.data,

        // Widened first: uint32 * uint16 multiplies in 32 bit arithmetic and would wrap.
        .size = (size_t)value->data.count * value->data.item_size,
    };
}

ff_string_view ff_value_as_string(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_string);
    return (ff_string_view)
    {
        .data = (const char*)value->data.data,
        .count = value->data.count,
    };
}

ff_value_span ff_value_as_array(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_array && value->data.item_size == sizeof(ff_value) && value->data.item_align == alignof(ff_value));
    return (ff_value_span)
    {
        .data = (ff_value*)value->data.data,
        .count = value->data.count,
    };
}
