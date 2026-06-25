#include "pch.h"
#include "base/array.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/math.h"
#include "base/value.h"

static_assert(sizeof(ff::value) == 24);
static_assert(sizeof(ff::ivalue) == 24);
static_assert(sizeof(ff::ivalue) == sizeof(ff::value)); // value and ivalue must stay layout-compatible

ff::value ff::value::new_empty()
{
    ff::value result{};
    result.type = ff::value_type::empty;
    return result;
}

ff::value ff::value::new_null()
{
    ff::value result{};
    result.type = ff::value_type::null;
    return result;
}

ff::value ff::value::new_boolean(bool value)
{
    ff::value result{};
    result.b = value;
    result.type = ff::value_type::boolean;
    return result;
}

ff::value ff::value::new_int32(int32_t value)
{
    ff::value result{};
    result.i32 = value;
    result.type = ff::value_type::int32;
    return result;
}

ff::value ff::value::new_int64(int64_t value)
{
    ff::value result{};
    result.i64 = value;
    result.type = ff::value_type::int64;
    return result;
}

ff::value ff::value::new_float32(float value)
{
    ff::value result{};
    result.f32 = value;
    result.type = ff::value_type::float32;
    return result;
}

ff::value ff::value::new_float64(double value)
{
    ff::value result{};
    result.f64 = value;
    result.type = ff::value_type::float64;
    return result;
}

ff::value ff::value::new_point_int32(int32_t x, int32_t y)
{
    ff::value result{};
    result.point_i32[0] = x;
    result.point_i32[1] = y;
    result.type = ff::value_type::point_int32;
    return result;
}

ff::value ff::value::new_point_int64(int64_t x, int64_t y)
{
    ff::value result{};
    result.point_i64[0] = x;
    result.point_i64[1] = y;
    result.type = ff::value_type::point_int64;
    return result;
}

ff::value ff::value::new_point_float32(float x, float y)
{
    ff::value result{};
    result.point_f32[0] = x;
    result.point_f32[1] = y;
    result.type = ff::value_type::point_float32;
    return result;
}

ff::value ff::value::new_point_float64(double x, double y)
{
    ff::value result{};
    result.point_f64[0] = x;
    result.point_f64[1] = y;
    result.type = ff::value_type::point_float64;
    return result;
}

ff::value ff::value::new_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    ff::value result{};
    result.rect_i32[0] = left;
    result.rect_i32[1] = top;
    result.rect_i32[2] = right;
    result.rect_i32[3] = bottom;
    result.type = ff::value_type::rect_int32;
    return result;
}

ff::value ff::value::new_rect_float32(float left, float top, float right, float bottom)
{
    ff::value result{};
    result.rect_f32[0] = left;
    result.rect_f32[1] = top;
    result.rect_f32[2] = right;
    result.rect_f32[3] = bottom;
    result.type = ff::value_type::rect_float32;
    return result;
}

ff::value ff::value::new_guid(const GUID& value)
{
    ff::value result{};
    result.guid = value;
    result.type = ff::value_type::guid;
    return result;
}

ff::value ff::value::new_dict(ff::dict* value)
{
    ff::value result{};
    result.data.data = value;
    result.type = ff::value_type::dict;
    return result;
}

ff::value ff::value::new_data(ff::raw_span value, ff::arena* copy_arena)
{
    ff::array_span as;
    as.data = value.data;
    as.count = value.size;
    as.item_size = 1;
    as.item_align = alignof(size_t);

    return ff::value::new_data(as, copy_arena);
}

ff::value ff::value::new_data(ff::array_span value, ff::arena* copy_arena)
{
    ff::value result{};
    result.data = value;

    if (copy_arena && value.data && value.count && value.item_size)
    {
        size_t bytes = value.count * value.item_size;
        void* copied = copy_arena->alloc(bytes, __max(value.item_align, alignof(size_t)));
        ::memcpy(copied, value.data, bytes);
        result.data.data = copied;
    }

    result.type = ff::value_type::data;
    return result;
}

ff::value ff::value::new_string(ff::string_view value, ff::arena* copy_arena)
{
    ff::raw_span span;
    span.data = value.data;
    span.size = value.count;

    ff::value result = ff::value::new_data(span, copy_arena);
    result.type = ff::value_type::string;
    return result;
}

ff::value ff::value::new_array(ff::value* values, size_t size, ff::arena* copy_arena)
{
    ff::array_span span;
    span.data = values;
    span.count = size;
    span.item_size = sizeof(ff::value);
    span.item_align = alignof(ff::value);

    ff::value result = ff::value::new_data(span, copy_arena);
    result.type = ff::value_type::array;
    return result;
}

ff::dict* ff::value::as_dict() const
{
    FF_ASSERT(type == ff::value_type::dict);
    return (ff::dict*)this->data.data;
}

ff::raw_span ff::value::as_data() const
{
    FF_ASSERT(type == ff::value_type::data || type == ff::value_type::string || type == ff::value_type::array);
    ff::raw_span result;
    result.data = this->data.data;
    result.size = this->data.count * this->data.item_size;
    return result;
}

ff::string_view ff::value::as_string() const
{
    FF_ASSERT(type == ff::value_type::string);

    ff::string_view result;
    result.data = (const char*)this->data.data;
    result.count = this->data.count;
    return result;
}

ff::span<ff::value> ff::value::as_array() const
{
    FF_ASSERT(type == ff::value_type::array);

    ff::span<ff::value> result;
    result.data = (ff::value*)this->data.data;
    result.count = this->data.count;
    return result;
}

ff::ivalue ff::ivalue::pack(const ff::value& source)
{
    ff::ivalue result{};
    result.type = source.type;

    switch (source.type)
    {
        case ff::value_type::data:
        case ff::value_type::dict:
        case ff::value_type::string:
        case ff::value_type::array:
            // TODO: reference types. Append the target bytes/elements into the owning idict's blob and
            // record result.data.offset/count (recursing for array/dict). This needs the blob writer
            // and base pointer, so it is finalized together with ff::dict::pack's blob construction.
            break;

        default:
            // Inline types (empty/null/boolean/guid/int*/float*/point*/rect*) share value's layout, so
            // copy the 16-byte inline payload unchanged (GUID is the largest inline member).
            ::memcpy(&result.guid, &source.guid, sizeof(source.guid));
            break;
    }

    return result;
}

ff::idict ff::ivalue::as_dict(const void* base) const
{
    FF_ASSERT(this->type == ff::value_type::dict);

    ff::idict result{};
    result.data = (const uint8_t*)base + this->data.offset;
    result.size = this->data.count; // TODO: nested-dict region size, finalized with ff::dict::pack
    return result;
}

ff::string_view ff::ivalue::as_string(const void* base) const
{
    FF_ASSERT(this->type == ff::value_type::string);

    ff::string_view result;
    result.data = (const char*)((const uint8_t*)base + this->data.offset);
    result.count = this->data.count;
    return result;
}

ff::span<ff::ivalue> ff::ivalue::as_array(const void* base) const
{
    FF_ASSERT(this->type == ff::value_type::array);

    ff::span<ff::ivalue> result;
    result.data = (const ff::ivalue*)((const uint8_t*)base + this->data.offset);
    result.count = this->data.count;
    return result;
}

