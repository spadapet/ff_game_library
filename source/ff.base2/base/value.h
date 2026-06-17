#pragma once

#include "../base/span.h"
#include "../base/string_view.h"

namespace ff
{
    struct arena;
    struct dict;
    struct idict;

    enum class value_type : uint32_t
    {
        empty,
        null,
        boolean,
        guid,

        int32,
        int64,
        float32,
        float64,

        point_int32,
        point_int64,
        point_float32,
        point_float64,

        rect_int32,
        rect_float32,

        data, // any binary data
        dict, // ff::dict*
        string, // char* (UTF-8, no null terminator)
        array, // ff::value*
    };

    struct value
    {
        static ff::value new_empty();
        static ff::value new_null();
        static ff::value new_boolean(bool value);
        static ff::value new_guid(const GUID& value);
        static ff::value new_int32(int32_t value);
        static ff::value new_int64(int64_t value);
        static ff::value new_float32(float value);
        static ff::value new_float64(double value);
        static ff::value new_point_int32(int32_t x, int32_t y);
        static ff::value new_point_int64(int64_t x, int64_t y);
        static ff::value new_point_float32(float x, float y);
        static ff::value new_point_float64(double x, double y);
        static ff::value new_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom);
        static ff::value new_rect_float32(float left, float top, float right, float bottom);
        static ff::value new_data(ff::raw_span value, ff::arena* copy_arena = nullptr);
        static ff::value new_data(ff::array_span value, ff::arena* copy_arena = nullptr);
        static ff::value new_dict(ff::dict* value);
        static ff::value new_string(ff::string_view value, ff::arena* copy_arena = nullptr);
        static ff::value new_array(ff::value* values, size_t size, ff::arena* copy_arena = nullptr);

        ff::dict* as_dict() const;
        ff::span<ff::value> as_array() const;
        ff::string_view as_string() const;

        union
        {
            bool b;
            GUID guid;

            int32_t i32;
            int64_t i64;
            float f32;
            double f64;

            int32_t point_i32[2];
            float point_f32[2];
            int64_t point_i64[2];
            double point_f64[2];

            int32_t rect_i32[4];
            float rect_f32[4];

            ff::array_span data_a;
        };

        ff::value_type type;
    };
}
