#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/math.h"
#include "base/value.h"

_Static_assert(sizeof(ff_value) == 24, "ff_value must stay 24 bytes");
_Static_assert(sizeof(ff_ivalue) == 24, "ff_ivalue must stay 24 bytes");
_Static_assert(sizeof(ff_ivalue) == sizeof(ff_value), "ff_value and ff_ivalue must stay layout-compatible");

ff_value ff_value_new_empty(void)
{
    ff_value result = { 0 };
    result.type = ff_value_type_empty;
    return result;
}

ff_value ff_value_new_null(void)
{
    ff_value result = { 0 };
    result.type = ff_value_type_null;
    return result;
}

ff_value ff_value_new_boolean(bool value)
{
    ff_value result = { 0 };
    result.b = value;
    result.type = ff_value_type_boolean;
    return result;
}

ff_value ff_value_new_int32(int32_t value)
{
    ff_value result = { 0 };
    result.i32 = value;
    result.type = ff_value_type_int32;
    return result;
}

ff_value ff_value_new_int64(int64_t value)
{
    ff_value result = { 0 };
    result.i64 = value;
    result.type = ff_value_type_int64;
    return result;
}

ff_value ff_value_new_float32(float value)
{
    ff_value result = { 0 };
    result.f32 = value;
    result.type = ff_value_type_float32;
    return result;
}

ff_value ff_value_new_float64(double value)
{
    ff_value result = { 0 };
    result.f64 = value;
    result.type = ff_value_type_float64;
    return result;
}

ff_value ff_value_new_point_int32(int32_t x, int32_t y)
{
    ff_value result = { 0 };
    result.point_i32[0] = x;
    result.point_i32[1] = y;
    result.type = ff_value_type_point_int32;
    return result;
}

ff_value ff_value_new_point_int64(int64_t x, int64_t y)
{
    ff_value result = { 0 };
    result.point_i64[0] = x;
    result.point_i64[1] = y;
    result.type = ff_value_type_point_int64;
    return result;
}

ff_value ff_value_new_point_float32(float x, float y)
{
    ff_value result = { 0 };
    result.point_f32[0] = x;
    result.point_f32[1] = y;
    result.type = ff_value_type_point_float32;
    return result;
}

ff_value ff_value_new_point_float64(double x, double y)
{
    ff_value result = { 0 };
    result.point_f64[0] = x;
    result.point_f64[1] = y;
    result.type = ff_value_type_point_float64;
    return result;
}

ff_value ff_value_new_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    ff_value result = { 0 };
    result.rect_i32[0] = left;
    result.rect_i32[1] = top;
    result.rect_i32[2] = right;
    result.rect_i32[3] = bottom;
    result.type = ff_value_type_rect_int32;
    return result;
}

ff_value ff_value_new_rect_float32(float left, float top, float right, float bottom)
{
    ff_value result = { 0 };
    result.rect_f32[0] = left;
    result.rect_f32[1] = top;
    result.rect_f32[2] = right;
    result.rect_f32[3] = bottom;
    result.type = ff_value_type_rect_float32;
    return result;
}

ff_value ff_value_new_guid(GUID value)
{
    ff_value result = { 0 };
    result.guid = value;
    result.type = ff_value_type_guid;
    return result;
}

ff_value ff_value_new_dict(ff_dict* value)
{
    ff_value result = { 0 };
    result.data.data = value;
    result.type = ff_value_type_dict;
    return result;
}

ff_value ff_value_new_data(struct ff_span value, ff_arena* copy_arena)
{
    struct ff_array_span as = { 0 };
    as.data = value.data;
    as.count = value.size;
    as.item_size = 1;
    as.item_align = _Alignof(size_t);

    return ff_value_new_data_array_span(as, copy_arena);
}

ff_value ff_value_new_data_array_span(struct ff_array_span value, ff_arena* copy_arena)
{
    ff_value result = { 0 };
    result.data = value;

    if (copy_arena && value.data && value.count && value.item_size)
    {
        size_t bytes = (size_t)value.count * (size_t)value.item_size;
        void* copied = ff_arena_alloc(copy_arena, bytes, ff_math_max_size(value.item_align, _Alignof(size_t)));
        memcpy(copied, value.data, bytes);
        result.data.data = copied;
    }

    result.type = ff_value_type_data;
    return result;
}

ff_value ff_value_new_string(ff_string_view value, ff_arena* copy_arena)
{
    struct ff_span span;
    span.data = value.data;
    span.size = value.count;

    ff_value result = ff_value_new_data(span, copy_arena);
    result.type = ff_value_type_string;
    return result;
}

ff_value ff_value_new_array(ff_value* values, size_t size, ff_arena* copy_arena)
{
    struct ff_array_span span = { 0 };
    span.data = values;
    span.count = size;
    span.item_size = sizeof(ff_value);
    span.item_align = _Alignof(ff_value);

    ff_value result = ff_value_new_data_array_span(span, copy_arena);
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
    struct ff_span result;
    result.data = value->data.data;
    result.size = (size_t)value->data.count * (size_t)value->data.item_size;
    return result;
}

ff_string_view ff_value_as_string(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_string);

    ff_string_view result;
    result.data = (const char*)value->data.data;
    result.count = value->data.count;
    return result;
}

ff_value_span ff_value_as_array(const ff_value* value)
{
    FF_ASSERT(value->type == ff_value_type_array);

    ff_value_span result;
    result.data = (const ff_value*)value->data.data;
    result.count = value->data.count;
    return result;
}

void ff_value_pack(ff_value* value, ff_ivalue* dest)
{
    dest->type = value->type;

    switch (value->type)
    {
        case ff_value_type_data:
        case ff_value_type_dict:
        case ff_value_type_string:
        case ff_value_type_array:
            // TODO: reference types. Append the target bytes/elements into the owning idict's blob and
            // record result.data.offset/count (recursing for array/dict). This needs the blob writer
            // and base pointer, so it is finalized together with ff_dict_pack's blob construction.
            break;

        default:
            // Inline types (empty/null/boolean/guid/int*/float*/point*/rect*) share value's layout, so
            // copy the 16-byte inline payload unchanged (GUID is the largest inline member).
            memcpy(&dest->guid, &value->guid, sizeof(value->guid));
            break;
    }
}

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_dict);

    ff_idict result = { 0 };
    result.data = (const uint8_t*)base + value->data.offset;
    result.size = value->data.count; // TODO: nested-dict region size, finalized with ff_dict_pack
    return result;
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_string);

    ff_string_view result;
    result.data = (const char*)((const uint8_t*)base + value->data.offset);
    result.count = value->data.count;
    return result;
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_array);

    ff_ivalue_span result;
    result.data = (const ff_ivalue*)((const uint8_t*)base + value->data.offset);
    result.count = value->data.count;
    return result;
}
