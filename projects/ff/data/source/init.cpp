#include "pch.h"
#include "bool_v.h"
#include "data_v.h"
#include "dict_v.h"
#include "double_v.h"
#include "fixed_v.h"
#include "float_v.h"
#include "init.h"
#include "int_v.h"
#include "null_v.h"
#include "point_v.h"
#include "rect_v.h"
#include "saved_data_v.h"
#include "size_v.h"
#include "string_v.h"
#include "uuid_v.h"
#include "value_vector.h"

ff::init_data::init_data()
{
    static struct one_time_init
    {
        one_time_init()
        {
            ff::value::register_type<ff::type::bool_type>("bool");
            ff::value::register_type<ff::type::dict_type>("dict");
            ff::value::register_type<ff::type::data_type>("data");
            ff::value::register_type<ff::type::double_type>("double");
            ff::value::register_type<ff::type::double_vector_type>("double_vector");
            ff::value::register_type<ff::type::fixed_type>("fixed");
            ff::value::register_type<ff::type::fixed_vector_type>("fixed_vector");
            ff::value::register_type<ff::type::float_type>("float");
            ff::value::register_type<ff::type::float_vector_type>("float_vector");
            ff::value::register_type<ff::type::int_type>("int");
            ff::value::register_type<ff::type::int_vector_type>("int_vector");
            ff::value::register_type<ff::type::null_type>("null");
            ff::value::register_type<ff::type::point_double_type>("point_double");
            ff::value::register_type<ff::type::point_fixed_type>("point_fixed");
            ff::value::register_type<ff::type::point_float_type>("point_float");
            ff::value::register_type<ff::type::point_int_type>("point_int");
            ff::value::register_type<ff::type::point_size_type>("point_size");
            ff::value::register_type<ff::type::rect_double_type>("rect_double");
            ff::value::register_type<ff::type::rect_fixed_type>("rect_fixed");
            ff::value::register_type<ff::type::rect_float_type>("rect_float");
            ff::value::register_type<ff::type::rect_int_type>("rect_int");
            ff::value::register_type<ff::type::rect_size_type>("rect_size");
            ff::value::register_type<ff::type::saved_data_type>("saved_data");
            ff::value::register_type<ff::type::size_type>("size");
            ff::value::register_type<ff::type::string_type>("string");
            ff::value::register_type<ff::type::string_vector_type>("string_vector");
            ff::value::register_type<ff::type::uuid_type>("uuid");
            ff::value::register_type<ff::type::value_vector_type>("value_vector");
        }
    } init;
}
