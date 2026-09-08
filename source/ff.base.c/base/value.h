#pragma once

#include "../base/span.h"
#include "../base/string.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;

// The numbering of these is written into saved ff_idict files, so it is part of that format: types
// may only be appended, never reordered or removed, and anything that walks every type (see
// idict.c) has to be updated to match. ff_idict's file prefix records how many types the writer
// knew about so that a mismatch is caught rather than silently misread.
typedef enum ff_value_type
{
    ff_value_type_empty,
    ff_value_type_null,
    ff_value_type_boolean,
    ff_value_type_guid,

    ff_value_type_int32,
    ff_value_type_int64,
    ff_value_type_float32,
    ff_value_type_float64,

    ff_value_type_point_int32,
    ff_value_type_point_int64,
    ff_value_type_point_float32,
    ff_value_type_point_float64,

    ff_value_type_rect_int32,
    ff_value_type_rect_float32,

    ff_value_type_data, // any binary data
    ff_value_type_dict, // ff_dict*
    ff_value_type_string, // char* (UTF-8, no null terminator)
    ff_value_type_array, // ff_value* plus count
} ff_value_type;

typedef struct ff_value
{
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

        struct ff_array_span data;
    };

    ff_value_type type;
} ff_value;

typedef struct ff_value_span
{
    ff_value* data;
    size_t count;
} ff_value_span;

ff_value ff_value_new_empty(void);
ff_value ff_value_new_null(void);
ff_value ff_value_new_boolean(bool value);
ff_value ff_value_new_guid(GUID value);
ff_value ff_value_new_int32(int32_t value);
ff_value ff_value_new_int64(int64_t value);
ff_value ff_value_new_float32(float value);
ff_value ff_value_new_float64(double value);
ff_value ff_value_new_point_int32(int32_t x, int32_t y);
ff_value ff_value_new_point_int64(int64_t x, int64_t y);
ff_value ff_value_new_point_float32(float x, float y);
ff_value ff_value_new_point_float64(double x, double y);
ff_value ff_value_new_rect_int32(int32_t left, int32_t top, int32_t right, int32_t bottom);
ff_value ff_value_new_rect_float32(float left, float top, float right, float bottom);
// Sizes and alignments are stored in the narrow fields of ff_array_span, so these refuse anything
// that would not survive the trip and return an empty value instead of one that has silently
// wrapped: at most UINT32_MAX items, an item size and alignment that each fit in 16 bits.
ff_value ff_value_new_data(ff_span value);
ff_value ff_value_new_data_array(ff_array_span value);
ff_value ff_value_new_dict(ff_dict* value);
ff_value ff_value_new_string(ff_string_view value);
ff_value ff_value_new_array(ff_value_span value);

ff_dict* ff_value_as_dict(const ff_value* value);
ff_span ff_value_as_data(const ff_value* value);
ff_string_view ff_value_as_string(const ff_value* value);
ff_value_span ff_value_as_array(const ff_value* value);
