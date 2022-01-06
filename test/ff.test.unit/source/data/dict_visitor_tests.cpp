#include "pch.h"

using namespace std::string_view_literals;

namespace ff::test::data
{
    TEST_CLASS(dict_visitor_tests)
    {
    public:
        TEST_METHOD(convert_number_to_string)
        {
            std::string json(
                "{\n"
                "  'foo': 1,\n"
                "  'obj' : { 'nested': { 'foo': 2 }, 'nested2': [ 3, 4, 5 ] },\n"
                "  'numbers' : [ -1, 0, 8.5, -98.76e54, 1E-8 ],\n"
                "  'identifiers' : [ true, false, null ],\n"
                "  'string' : [ 'Hello', 'World', 123 ],\n"
                "}\n");
            std::replace(json.begin(), json.end(), '\'', '\"');

            std::string expect_json(
                "{\r\n"
                "  'foo': '1',\r\n"
                "  'identifiers':\r\n"
                "  [\r\n"
                "     true,\r\n"
                "     false,\r\n"
                "     null\r\n"
                "  ],\r\n"
                "  'numbers':\r\n"
                "  [\r\n"
                "     '-1',\r\n"
                "     '0',\r\n"
                "     '8.5',\r\n"
                "     '-9.876e+55',\r\n"
                "     '1e-08'\r\n"
                "  ],\r\n"
                "  'obj':\r\n"
                "  {\r\n"
                "    'nested':\r\n"
                "    {\r\n"
                "      'foo': '2'\r\n"
                "    },\r\n"
                "    'nested2':\r\n"
                "    [\r\n"
                "       '3',\r\n"
                "       '4',\r\n"
                "       '5'\r\n"
                "    ]\r\n"
                "  },\r\n"
                "  'string':\r\n"
                "  [\r\n"
                "     'Hello',\r\n"
                "     'World',\r\n"
                "     '123'\r\n"
                "  ]\r\n"
                "}");
            std::replace(expect_json.begin(), expect_json.end(), '\'', '\"');

            ff::dict dict;
            ff::json_parse(json, dict);

            class visitor : public ff::dict_visitor_base
            {
            protected:
                virtual ff::value_ptr transform_value(ff::value_ptr value) override
                {
                    if (value->is_type<int>())
                    {
                        std::ostringstream str;
                        str << value->get<int>();
                        return ff::value::create<std::string>(str.str());
                    }
                    else if (value->is_type<double>())
                    {
                        std::ostringstream str;
                        str << value->get<double>();
                        return ff::value::create<std::string>(str.str());
                    }

                    return ff::dict_visitor_base::transform_value(value);
                }
            };

            visitor v;
            std::vector<std::string> errors;
            ff::dict new_dict = v.visit_dict(dict, errors)->get<ff::dict>();

            std::ostringstream new_json;
            ff::json_write(new_dict, new_json);
            Assert::AreEqual(expect_json, new_json.str());
        }
    };
}
