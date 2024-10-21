#include "pch.h"

namespace ff::test::data
{
    TEST_CLASS(json_tests)
    {
    public:
        TEST_METHOD(json_tokenizer_test)
        {
            std::string json(
                "{\n"
                "  // Test comment\n"
                "  /* Another test comment */\n"
                "  'foo': 'bar',\n"
                "  'obj' : { 'nested': {}, 'nested2': [] },\n"
                "  'numbers' : [ -1, 0, 8.5, -98.76e54, 1E-8 ],\n"
                "  'identifiers' : [ true, false, null ],\n"
                "  'string' : [ 'Hello', 'a\\'\\r\\u0020z' ],\n"
                "}\n");
            std::replace(json.begin(), json.end(), '\'', '\"');
            ff::internal::json_tokenizer tokenizer(json);

            for (ff::internal::json_token token = tokenizer.next();
                token.type != ff::internal::json_token_type::none;
                token = tokenizer.next())
            {
                Assert::IsTrue(token.type != ff::internal::json_token_type::error);

                ff::value_ptr value;

                switch (token.type)
                {
                    case ff::internal::json_token_type::number_token:
                        value = token.get();
                        Assert::IsTrue(value->is_type<double>() || value->is_type<int>());
                        break;

                    case ff::internal::json_token_type::string_token:
                        value = token.get();
                        Assert::IsTrue(value->is_type<std::string>());
                        break;

                    case ff::internal::json_token_type::null_token:
                        value = token.get();
                        Assert::IsTrue(value->is_type<nullptr_t>());
                        break;

                    case ff::internal::json_token_type::true_token:
                        value = token.get();
                        Assert::IsTrue(value->is_type<bool>() && value->get<bool>());
                        break;

                    case ff::internal::json_token_type::false_token:
                        value = token.get();
                        Assert::IsTrue(value->is_type<bool>() && !value->get<bool>());
                        break;
                }
            }
        }

        TEST_METHOD(json_parser_test)
        {
            std::string json(
                "{\n"
                "  // Test comment\n"
                "  /* Another test comment */\n"
                "  'foo': 'bar',\n"
                "  'obj' : { 'nested': {}, 'nested2': [] },\n"
                "  'numbers' : [ -1, 0, 8.5, -98.76e54, 1E-8 ],\n"
                "  'identifiers' : [ true, false, null ],\n"
                "  'string' : [ 'Hello', 'a\\'\\r\\u0020z' ],\n"
                "}\n");
            std::replace(json.begin(), json.end(), '\'', '\"');

            const char* error_pos;
            ff::dict dict;
            ff::json_parse(json, dict, &error_pos);

            Assert::AreEqual<size_t>(5, dict.size());
            std::vector<std::string_view> names = dict.child_names();
            std::sort(names.begin(), names.end());
            Assert::IsTrue(names[0] == "foo");
            Assert::IsTrue(names[1] == "identifiers");
            Assert::IsTrue(names[2] == "numbers");
            Assert::IsTrue(names[3] == "obj");
            Assert::IsTrue(names[4] == "string");

            Assert::IsTrue(dict.get("foo")->is_type<std::string>());
            Assert::IsTrue(dict.get("foo")->get<std::string>() == "bar");
        }

        TEST_METHOD(json_print_test)
        {
            std::string json(
                "{\n"
                "  // Test comment\n"
                "  /* Another test comment */\n"
                "  'foo': 'bar',\n"
                "  'obj' : { 'nested': {}, 'nested2': [] },\n"
                "  'numbers' : [ -1, 0, 8.5, -98.76e54, 1E-8 ],\n"
                "  'identifiers' : [ true, false, null ],\n"
                "  'string' : [ 'Hello', 'a\\'\\r\\u0020z' ],\n"
                "}\n");
            std::replace(json.begin(), json.end(), '\'', '\"');

            ff::dict dict;
            ff::json_parse(json, dict);

            std::ostringstream actual_output;
            ff::json_write(dict, actual_output);
            std::string actual = actual_output.str();
            std::string expect(
                "{\r\n"
                "  'foo': 'bar',\r\n"
                "  'identifiers':\r\n"
                "  [\r\n"
                "     true,\r\n"
                "     false,\r\n"
                "     null\r\n"
                "  ],\r\n"
                "  'numbers':\r\n"
                "  [\r\n"
                "     -1,\r\n"
                "     0,\r\n"
                "     8.5,\r\n"
                "     -9.876e+55,\r\n"
                "     1e-08\r\n"
                "  ],\r\n"
                "  'obj':\r\n"
                "  {\r\n"
                "    'nested': {},\r\n"
                "    'nested2': []\r\n"
                "  },\r\n"
                "  'string':\r\n"
                "  [\r\n"
                "     'Hello',\r\n"
                "     'a\\'\\r z'\r\n"
                "  ]\r\n"
                "}");
            std::replace(expect.begin(), expect.end(), '\'', '\"');

            Assert::AreEqual(expect, actual);
        }

        TEST_METHOD(JsonDeepValue)
        {
            std::string json(
                "{\n"
                "  'first': [ 1, 'two', { 'three': [ 10, 20, 30 ] } ],\n"
                "  'second': { 'nested': { 'nested2': { 'numbers': [ 100, 200, 300 ] } } },\n"
                "  'arrays': [ [ 1, { 'two': 2 } ], [ 3, 4 ], [ 5, 6, [ 7, 8 ] ] ]\n"
                "}\n");
            std::replace(json.begin(), json.end(), '\'', '\"');

            ff::dict dict;
            ff::json_parse(json, dict);

            ff::value_ptr value;
            value = dict.get("/null");
            Assert::IsTrue(!value);

            value = dict.get("/first");
            Assert::IsTrue(value && value->is_type<ff::value_vector>());

            value = dict.get("/first/");
            Assert::IsTrue(!value);

            value = dict.get("/first[1]");
            Assert::IsTrue(value && value->is_type<std::string>());

            value = dict.get("/first[1]/");
            Assert::IsTrue(!value);

            value = dict.get("/first[2]/null");
            Assert::IsTrue(!value);

            value = dict.get("/first[2]/three");
            Assert::IsTrue(value && value->is_type<ff::value_vector>());

            value = dict.get("/first[2]/three[1]");
            Assert::IsTrue(value && value->is_type<int>());

            value = dict.get("/arrays[0][1]/two");
            Assert::IsTrue(value && value->is_type<int>());

            value = dict.get("/arrays[2]");
            Assert::IsTrue(value && value->is_type<ff::value_vector>());

            value = dict.get("/arrays[2][2]");
            Assert::IsTrue(value && value->is_type<ff::value_vector>());

            value = dict.get("/arrays[2][2][1]");
            Assert::IsTrue(value && value->is_type<int>());
        }
    };
}
