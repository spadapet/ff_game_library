#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/variant.h"

// A variant is POD: every factory value-initializes the result ('ff::variant value{}'), which zeroes
// the whole struct (tag 'empty', pointer ref mode), then fills in the tag and the single payload
// member that matches it. Reference types (string, data, array, dict, 64-bit rects) go through
// 'ref', which holds an absolute pointer by default; ff::variant::to_offset converts that to a byte
// offset for binary serialization, and to_pointer converts it back after loading. A variant never
// owns or frees memory; the bytes it references live in an arena.

static_assert(sizeof(ff::variant) == 24, "ff::variant should stay 24 bytes");
static_assert(alignof(ff::variant) == 8, "ff::variant should stay 8-byte aligned");
static_assert(sizeof(GUID) == 16, "GUID is the largest inline variant payload");
static_assert((uint32_t)ff::variant_type::empty == 0, "a zeroed variant must be 'empty'");
static_assert((uint8_t)ff::variant_ref_mode::pointer == 0, "a zeroed variant must be in pointer mode");

bool ff::variant::is(ff::variant_type type) const
{
    return this->type == type;
}

bool ff::variant::is_reference() const
{
    switch (this->type)
    {
        case ff::variant_type::string:
        case ff::variant_type::data:
        case ff::variant_type::array:
        case ff::variant_type::dict:
        case ff::variant_type::rect_int64:
        case ff::variant_type::rect_uint64:
        case ff::variant_type::rect_float64:
            return true;

        default:
            return false;
    }
}

const void* ff::variant::ref_ptr(const void* base) const
{
    FF_CHECK_RET_VAL(this->is_reference(), nullptr);
    return (this->ref_mode == ff::variant_ref_mode::offset)
        ? (const void*)((const uint8_t*)base + this->ref.offset)
        : this->ref.ptr;
}

ff::string_view ff::variant::as_string(const void* base) const
{
    ff::string_view result{ "", 0 };
    FF_CHECK_RET_VAL(this->type == ff::variant_type::string, result);
    result.data = (const char*)this->ref_ptr(base);
    result.size = this->ref.size;
    return result;
}

ff::blob ff::variant::as_data(const void* base) const
{
    ff::blob result{ nullptr, 0 };
    FF_CHECK_RET_VAL(this->type == ff::variant_type::data, result);
    result.data = this->ref_ptr(base);
    result.size = this->ref.size;
    return result;
}

const ff::variant* ff::variant::as_array(size_t& count, const void* base) const
{
    count = 0;
    FF_CHECK_RET_VAL(this->type == ff::variant_type::array, nullptr);
    count = this->ref.size;
    return (const ff::variant*)this->ref_ptr(base);
}

const ff::dict* ff::variant::as_dict(const void* base) const
{
    FF_CHECK_RET_VAL(this->type == ff::variant_type::dict, nullptr);
    return (const ff::dict*)this->ref_ptr(base);
}

const int64_t* ff::variant::as_rect_int64(const void* base) const
{
    FF_CHECK_RET_VAL(this->type == ff::variant_type::rect_int64, nullptr);
    return (const int64_t*)this->ref_ptr(base);
}

const uint64_t* ff::variant::as_rect_uint64(const void* base) const
{
    FF_CHECK_RET_VAL(this->type == ff::variant_type::rect_uint64, nullptr);
    return (const uint64_t*)this->ref_ptr(base);
}

const double* ff::variant::as_rect_float64(const void* base) const
{
    FF_CHECK_RET_VAL(this->type == ff::variant_type::rect_float64, nullptr);
    return (const double*)this->ref_ptr(base);
}

void ff::variant::to_offset(const void* base)
{
    if (this->is_reference() && this->ref_mode == ff::variant_ref_mode::pointer)
    {
        // Reads ref.ptr before writing the overlapping ref.offset, so the in-place swap is safe.
        this->ref.offset = (uint64_t)((const uint8_t*)this->ref.ptr - (const uint8_t*)base);
        this->ref_mode = ff::variant_ref_mode::offset;
    }
}

void ff::variant::to_pointer(const void* base)
{
    if (this->is_reference() && this->ref_mode == ff::variant_ref_mode::offset)
    {
        this->ref.ptr = (const uint8_t*)base + this->ref.offset;
        this->ref_mode = ff::variant_ref_mode::pointer;
    }
}

ff::variant ff::variant_empty()
{
    ff::variant value{};
    value.type = ff::variant_type::empty;
    return value;
}

ff::variant ff::variant_null()
{
    ff::variant value{};
    value.type = ff::variant_type::null;
    return value;
}

ff::variant ff::variant_boolean(bool b)
{
    ff::variant value{};
    value.type = ff::variant_type::boolean;
    value.b = b;
    return value;
}

ff::variant ff::variant_int32(int32_t i32)
{
    ff::variant value{};
    value.type = ff::variant_type::int32;
    value.i32 = i32;
    return value;
}

ff::variant ff::variant_uint32(uint32_t u32)
{
    ff::variant value{};
    value.type = ff::variant_type::uint32;
    value.u32 = u32;
    return value;
}

ff::variant ff::variant_int64(int64_t i64)
{
    ff::variant value{};
    value.type = ff::variant_type::int64;
    value.i64 = i64;
    return value;
}

ff::variant ff::variant_uint64(uint64_t u64)
{
    ff::variant value{};
    value.type = ff::variant_type::uint64;
    value.u64 = u64;
    return value;
}

ff::variant ff::variant_float32(float f32)
{
    ff::variant value{};
    value.type = ff::variant_type::float32;
    value.f32 = f32;
    return value;
}

ff::variant ff::variant_float64(double f64)
{
    ff::variant value{};
    value.type = ff::variant_type::float64;
    value.f64 = f64;
    return value;
}

ff::variant ff::variant_point_int32(int32_t x, int32_t y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_int32;
    value.xy_i32[0] = x;
    value.xy_i32[1] = y;
    return value;
}

ff::variant ff::variant_point_uint32(uint32_t x, uint32_t y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_uint32;
    value.xy_u32[0] = x;
    value.xy_u32[1] = y;
    return value;
}

ff::variant ff::variant_point_int64(int64_t x, int64_t y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_int64;
    value.xy_i64[0] = x;
    value.xy_i64[1] = y;
    return value;
}

ff::variant ff::variant_point_uint64(uint64_t x, uint64_t y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_uint64;
    value.xy_u64[0] = x;
    value.xy_u64[1] = y;
    return value;
}

ff::variant ff::variant_point_float32(float x, float y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_float32;
    value.xy_f32[0] = x;
    value.xy_f32[1] = y;
    return value;
}

ff::variant ff::variant_point_float64(double x, double y)
{
    ff::variant value{};
    value.type = ff::variant_type::point_float64;
    value.xy_f64[0] = x;
    value.xy_f64[1] = y;
    return value;
}

ff::variant ff::variant_size_int32(int32_t width, int32_t height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_int32;
    value.xy_i32[0] = width;
    value.xy_i32[1] = height;
    return value;
}

ff::variant ff::variant_size_uint32(uint32_t width, uint32_t height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_uint32;
    value.xy_u32[0] = width;
    value.xy_u32[1] = height;
    return value;
}

ff::variant ff::variant_size_int64(int64_t width, int64_t height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_int64;
    value.xy_i64[0] = width;
    value.xy_i64[1] = height;
    return value;
}

ff::variant ff::variant_size_uint64(uint64_t width, uint64_t height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_uint64;
    value.xy_u64[0] = width;
    value.xy_u64[1] = height;
    return value;
}

ff::variant ff::variant_size_float32(float width, float height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_float32;
    value.xy_f32[0] = width;
    value.xy_f32[1] = height;
    return value;
}

ff::variant ff::variant_size_float64(double width, double height)
{
    ff::variant value{};
    value.type = ff::variant_type::size_float64;
    value.xy_f64[0] = width;
    value.xy_f64[1] = height;
    return value;
}

ff::variant ff::variant_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    ff::variant value{};
    value.type = ff::variant_type::rect_int32;
    value.rect_i32[0] = left;
    value.rect_i32[1] = top;
    value.rect_i32[2] = right;
    value.rect_i32[3] = bottom;
    return value;
}

ff::variant ff::variant_rect_uint32(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
    ff::variant value{};
    value.type = ff::variant_type::rect_uint32;
    value.rect_u32[0] = left;
    value.rect_u32[1] = top;
    value.rect_u32[2] = right;
    value.rect_u32[3] = bottom;
    return value;
}

ff::variant ff::variant_rect_float32(float left, float top, float right, float bottom)
{
    ff::variant value{};
    value.type = ff::variant_type::rect_float32;
    value.rect_f32[0] = left;
    value.rect_f32[1] = top;
    value.rect_f32[2] = right;
    value.rect_f32[3] = bottom;
    return value;
}

static ff::variant variant_rect_large(ff::arena* arena, ff::variant_type type, const void* components, size_t size)
{
    ff::variant value{};
    value.type = ff::variant_type::empty;
    FF_ASSERT_RET_VAL(arena, value);

    void* dest = arena->alloc(size, alignof(uint64_t));
    FF_ASSERT_RET_VAL(dest, value);

    ::memcpy(dest, components, size);
    value.type = type;
    value.ref.ptr = dest;
    return value;
}

ff::variant ff::variant_rect_int64(ff::arena* arena, int64_t left, int64_t top, int64_t right, int64_t bottom)
{
    const int64_t components[4] = { left, top, right, bottom };
    return ::variant_rect_large(arena, ff::variant_type::rect_int64, components, sizeof(components));
}

ff::variant ff::variant_rect_uint64(ff::arena* arena, uint64_t left, uint64_t top, uint64_t right, uint64_t bottom)
{
    const uint64_t components[4] = { left, top, right, bottom };
    return ::variant_rect_large(arena, ff::variant_type::rect_uint64, components, sizeof(components));
}

ff::variant ff::variant_rect_float64(ff::arena* arena, double left, double top, double right, double bottom)
{
    const double components[4] = { left, top, right, bottom };
    return ::variant_rect_large(arena, ff::variant_type::rect_float64, components, sizeof(components));
}

ff::variant ff::variant_guid(const GUID& guid)
{
    ff::variant value{};
    value.type = ff::variant_type::guid;
    value.guid = guid;
    return value;
}

ff::variant ff::variant_string(ff::string_view str)
{
    ff::variant value{};
    value.type = ff::variant_type::string;
    value.ref.ptr = str.data;
    value.ref.size = str.size;
    return value;
}

ff::variant ff::variant_data(ff::blob blob)
{
    ff::variant value{};
    value.type = ff::variant_type::data;
    value.ref.ptr = blob.data;
    value.ref.size = blob.size;
    return value;
}

ff::variant ff::variant_array(const ff::variant* items, size_t count)
{
    ff::variant value{};
    value.type = ff::variant_type::array;
    value.ref.ptr = items;
    value.ref.size = count;
    return value;
}

ff::variant ff::variant_string(ff::arena* arena, ff::string_view str)
{
    ff::variant value{};
    value.type = ff::variant_type::empty;
    FF_ASSERT_RET_VAL(arena, value);

    // One extra byte holds a null terminator so 'data' is a valid C string; 'size' still excludes it.
    char* dest = (char*)arena->alloc(str.size + 1, alignof(char));
    FF_ASSERT_RET_VAL(dest, value);

    if (str.size)
    {
        ::memcpy(dest, str.data, str.size);
    }

    dest[str.size] = 0;

    value.type = ff::variant_type::string;
    value.ref.ptr = dest;
    value.ref.size = str.size;
    return value;
}

ff::variant ff::variant_data(ff::arena* arena, const void* data, size_t size)
{
    ff::variant value{};
    value.type = ff::variant_type::empty;
    FF_ASSERT_RET_VAL(arena, value);
    FF_ASSERT_RET_VAL(data || !size, value);

    void* dest = nullptr;
    if (size)
    {
        dest = arena->alloc(size, alignof(uint64_t));
        FF_ASSERT_RET_VAL(dest, value);
        ::memcpy(dest, data, size);
    }

    value.type = ff::variant_type::data;
    value.ref.ptr = dest;
    value.ref.size = size;
    return value;
}

ff::variant ff::variant_array(ff::arena* arena, const ff::variant* items, size_t count)
{
    ff::variant value{};
    value.type = ff::variant_type::empty;
    FF_ASSERT_RET_VAL(arena, value);
    FF_ASSERT_RET_VAL(items || !count, value);
    FF_ASSERT_RET_VAL(count <= SIZE_MAX / sizeof(ff::variant), value);

    ff::variant* dest = nullptr;
    if (count)
    {
        dest = (ff::variant*)arena->alloc(count * sizeof(ff::variant), alignof(ff::variant));
        FF_ASSERT_RET_VAL(dest, value);
        ::memcpy(dest, items, count * sizeof(ff::variant));
    }

    value.type = ff::variant_type::array;
    value.ref.ptr = dest;
    value.ref.size = count;
    return value;
}

ff::variant ff::variant_dict(const ff::dict* dict)
{
    ff::variant value{};
    value.type = ff::variant_type::dict;
    value.ref.ptr = dict;
    return value;
}
