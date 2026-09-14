#pragma once

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
    ff_value_type_idict, // ff_idict
    ff_value_type_string, // char* (UTF-8, no null terminator)
    ff_value_type_array, // value array

    ff_value_type_count, // always last
} ff_value_type;
