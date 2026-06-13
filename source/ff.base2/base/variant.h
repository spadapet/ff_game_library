#pragma once

#include "../base/blob.h"
#include "../base/string_view.h"

namespace ff
{
    struct arena;
    struct dict; // not implemented yet; a variant only references one by pointer or offset

    // Discriminant for ff::variant. Values are persisted by binary serialization, so add new entries
    // at the end and never reorder. 'empty' is 0 so a zero-initialized (or memset) variant is empty.
    enum class variant_type : uint32_t
    {
        empty, // no value present
        null,  // an explicit null value, distinct from 'empty'
        boolean,

        int32,
        uint32,
        int64,
        uint64,
        float32,
        float64,

        // 2-component (x, y). Stored inline.
        point_int32,
        point_uint32,
        point_int64,
        point_uint64,
        point_float32,
        point_float64,

        // 2-component (width, height); conventionally non-negative. Stored inline.
        size_int32,
        size_uint32,
        size_int64,
        size_uint64,
        size_float32,
        size_float64,

        // 4-component (left, top, right, bottom). 32-bit rects are inline; 64-bit rects live in an arena.
        rect_int32,
        rect_uint32,
        rect_int64,
        rect_uint64,
        rect_float32,
        rect_float64,

        guid,   // 16 bytes, stored inline
        string, // UTF-8 bytes in stable (arena) storage
        data,   // ff::blob of bytes in stable (arena) storage
        array,  // contiguous ff::variant elements in stable (arena) storage
        dict,   // a sub-dictionary in stable (arena) storage
    };

    // How a variant's reference payload is interpreted. 'pointer' is an absolute address (normal
    // in-memory use); 'offset' is a byte offset from a base the owning dict provides, so the dict and
    // everything it references can sit in one arena and be binary-copied to disk, then resolved against
    // a new base after loading. 'pointer' is 0 so a zero-initialized variant is in pointer mode.
    enum class variant_ref_mode : uint8_t
    {
        pointer,
        offset,
    };

    // Reference payload for the variable-sized / oversized types (string, data, array, dict, and the
    // 64-bit rects). Holds either an absolute pointer or a byte offset (see ff::variant_ref_mode) plus
    // a count: byte length for string/data, element count for array, unused for dict and rects.
    struct variant_ref
    {
        union
        {
            const void* ptr;
            uint64_t offset;
        };

        size_t size;
    };

    // POD tagged union for data-dictionary values. There are no constructors or destructors: copy it by
    // value, and value-initializing it ('ff::variant v{}') or zeroing its bytes yields a valid 'empty'
    // variant in pointer mode. Anything it references (string, data, array, 64-bit rect, sub-dictionary)
    // lives in an arena, so a variant never owns or frees memory.
    //
    // Layout note (answers "how do I keep the tag from breaking alignment of float64?"): the 16-byte
    // payload union comes first, so it sits at offset 0 with 8-byte alignment, which is exactly what
    // double / int64 / 64-bit points need. The 4-byte 'type' tag and 1-byte 'ref_mode' follow in the
    // trailing padding that 8-byte alignment forces anyway, so the whole struct is 24 bytes. GUID is the
    // largest inline value (16 bytes) - enough for a rect of int32/float32 or a point/size of
    // int64/double, but not a rect of int64/double (32 bytes). Those are referenced through 'ref'.
    struct variant
    {
        bool is(ff::variant_type type) const;
        bool is_reference() const; // true for string, data, array, dict, and 64-bit rects

        // Resolve a reference variant's payload to an absolute pointer. In pointer mode 'base' is
        // ignored; in offset mode the result is 'base + offset'.
        const void* ref_ptr(const void* base = nullptr) const;

        // Typed reference accessors. In pointer mode 'base' is ignored; in offset mode pass the base
        // the owning dict provides. Each returns an empty/null result on a type mismatch.
        ff::string_view as_string(const void* base = nullptr) const;
        ff::blob as_data(const void* base = nullptr) const;
        const ff::variant* as_array(size_t& count, const void* base = nullptr) const;
        const ff::dict* as_dict(const void* base = nullptr) const;
        const int64_t* as_rect_int64(const void* base = nullptr) const;
        const uint64_t* as_rect_uint64(const void* base = nullptr) const;
        const double* as_rect_float64(const void* base = nullptr) const;

        // Relocate this variant's reference between an absolute pointer and a byte offset from 'base'.
        // No-op for non-reference types and for variants already in the requested mode, so a dict can
        // call these on every value while serializing (to_offset) or after loading (to_pointer).
        void to_offset(const void* base);
        void to_pointer(const void* base);

        union
        {
            bool b;

            int32_t i32;
            uint32_t u32;
            int64_t i64;
            uint64_t u64;
            float f32;
            double f64;

            // points and sizes; read the member that matches 'type'
            int32_t xy_i32[2];
            uint32_t xy_u32[2];
            float xy_f32[2];
            int64_t xy_i64[2];
            uint64_t xy_u64[2];
            double xy_f64[2];

            // 32-bit rects (left, top, right, bottom)
            int32_t rect_i32[4];
            uint32_t rect_u32[4];
            float rect_f32[4];

            GUID guid;

            // string, data, array, dict, and 64-bit rects (absolute pointer or arena offset)
            ff::variant_ref ref;
        };

        ff::variant_type type;
        ff::variant_ref_mode ref_mode;
    };

    ff::variant variant_empty();
    ff::variant variant_null();
    ff::variant variant_boolean(bool value);

    ff::variant variant_int32(int32_t value);
    ff::variant variant_uint32(uint32_t value);
    ff::variant variant_int64(int64_t value);
    ff::variant variant_uint64(uint64_t value);
    ff::variant variant_float32(float value);
    ff::variant variant_float64(double value);

    ff::variant variant_point_int32(int32_t x, int32_t y);
    ff::variant variant_point_uint32(uint32_t x, uint32_t y);
    ff::variant variant_point_int64(int64_t x, int64_t y);
    ff::variant variant_point_uint64(uint64_t x, uint64_t y);
    ff::variant variant_point_float32(float x, float y);
    ff::variant variant_point_float64(double x, double y);

    ff::variant variant_size_int32(int32_t width, int32_t height);
    ff::variant variant_size_uint32(uint32_t width, uint32_t height);
    ff::variant variant_size_int64(int64_t width, int64_t height);
    ff::variant variant_size_uint64(uint64_t width, uint64_t height);
    ff::variant variant_size_float32(float width, float height);
    ff::variant variant_size_float64(double width, double height);

    ff::variant variant_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom);
    ff::variant variant_rect_uint32(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
    ff::variant variant_rect_float32(float left, float top, float right, float bottom);

    // 64-bit rects are 32 bytes, too large to store inline, so they are copied into 'arena'.
    ff::variant variant_rect_int64(ff::arena* arena, int64_t left, int64_t top, int64_t right, int64_t bottom);
    ff::variant variant_rect_uint64(ff::arena* arena, uint64_t left, uint64_t top, uint64_t right, uint64_t bottom);
    ff::variant variant_rect_float64(ff::arena* arena, double left, double top, double right, double bottom);

    ff::variant variant_guid(const GUID& value);

    // References stable storage; the caller guarantees the bytes/elements outlive the variant (e.g. an arena).
    ff::variant variant_string(ff::string_view value);
    ff::variant variant_data(ff::blob value);
    ff::variant variant_array(const ff::variant* items, size_t count);
    ff::variant variant_dict(const ff::dict* value);

    // Copies the bytes/elements into 'arena' so the variant owns a stable, lifetime-safe reference.
    ff::variant variant_string(ff::arena* arena, ff::string_view value);
    ff::variant variant_data(ff::arena* arena, const void* data, size_t size);
    ff::variant variant_array(ff::arena* arena, const ff::variant* items, size_t count);
}
