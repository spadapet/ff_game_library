#pragma once

#include "../base/span.h"
#include "../base/string.h"
#include "../base/value.h"

typedef struct ff_idict ff_idict;

typedef struct ff_ivalue
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

        struct ff_array_slice data;
    };

    ff_value_type type;
} ff_ivalue;

typedef struct ff_ivalue_span
{
    const ff_ivalue* data;
    size_t count;
} ff_ivalue_span;

ff_array_span ff_ivalue_as_data(const ff_ivalue* value, const ff_idict* parent_dict);
ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict);
ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict);
ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict);
