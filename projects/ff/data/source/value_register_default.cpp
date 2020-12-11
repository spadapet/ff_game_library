#include "pch.h"
#include "bool_v.h"
#include "dict_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "int_v.h"
#include "null_v.h"
#include "point_v.h"
#include "rect_v.h"
#include "size_v.h"
#include "string_v.h"
#include "uuid_v.h"
#include "value_register_default.h"
#include "value_vector.h"

// These are persisted, so don't ever change them if you need to load old data
enum class lookup_id : uint8_t
{
    none = 0,
    bool_type = 1,
    dict_type = 2,
    double_type = 3,
    double_vector_type = 4,
    fixed_type = 5,
    fixed_vector_type = 6,
    float_type = 7,
    float_vector_type = 8,
    int_type = 9,
    int_vector_type = 10,
    null_type = 11,
    point_double_type = 12,
    point_fixed_type = 13,
    point_float_type = 14,
    point_int_type = 15,
    point_size_type = 16,
    rect_double_type = 17,
    rect_fixed_type = 18,
    rect_float_type = 19,
    rect_int_type = 20,
    rect_size_type = 21,
    size_type = 22,
    string_type = 23,
    string_vector_type = 24,
    uuid_type = 25,
    value_vector_type = 26,
};

template<class T>
static void register_type(std::string_view type_name, ::lookup_id type_lookup_id)
{
    ff::value::register_type<T>(type_name, static_cast<uint32_t>(type_lookup_id));
}

ff::internal::value_register_default::value_register_default()
{
    using namespace ff::type;

    ::register_type<bool_type>("bool", ::lookup_id::bool_type);
    ::register_type<dict_type>("dict", ::lookup_id::dict_type);
    ::register_type<double_type>("double", ::lookup_id::double_type);
    ::register_type<double_vector_type>("double_vector", ::lookup_id::double_vector_type);
    ::register_type<fixed_type>("fixed", ::lookup_id::fixed_type);
    ::register_type<fixed_vector_type>("fixed_vector", ::lookup_id::fixed_vector_type);
    ::register_type<float_type>("float", ::lookup_id::float_type);
    ::register_type<float_vector_type>("float_vector", ::lookup_id::float_vector_type);
    ::register_type<int_type>("int", ::lookup_id::int_type);
    ::register_type<int_vector_type>("int_vector", ::lookup_id::int_vector_type);
    ::register_type<null_type>("null", ::lookup_id::null_type);
    ::register_type<point_double_type>("point_double", ::lookup_id::point_double_type);
    ::register_type<point_fixed_type>("point_fixed", ::lookup_id::point_fixed_type);
    ::register_type<point_float_type>("point_float", ::lookup_id::point_float_type);
    ::register_type<point_int_type>("point_int", ::lookup_id::point_int_type);
    ::register_type<point_size_type>("point_size", ::lookup_id::point_size_type);
    ::register_type<rect_double_type>("rect_double", ::lookup_id::rect_double_type);
    ::register_type<rect_fixed_type>("rect_fixed", ::lookup_id::rect_fixed_type);
    ::register_type<rect_float_type>("rect_float", ::lookup_id::rect_float_type);
    ::register_type<rect_int_type>("rect_int", ::lookup_id::rect_int_type);
    ::register_type<rect_size_type>("rect_size", ::lookup_id::rect_size_type);
    ::register_type<size_type>("size", ::lookup_id::size_type);
    ::register_type<string_type>("string", ::lookup_id::string_type);
    ::register_type<string_vector_type>("string_vector", ::lookup_id::string_vector_type);
    ::register_type<uuid_type>("uuid", ::lookup_id::uuid_type);
    ::register_type<value_vector_type>("value_vector", ::lookup_id::value_vector_type);
}
