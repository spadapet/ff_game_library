#include "pch.h"

namespace ff::test::data_persist
{
    TEST_CLASS(json_tests)
    {
    public:
        static ff_string_view sv(const char* text)
        {
            return ff_sz_view(text);
        }

        static bool view_equals(ff_string_view view, const char* text)
        {
            size_t count = ::strlen(text);
            return view.count == count && ::memcmp(view.data, text, count) == 0;
        }

        // Wraps a single JSON value in the smallest document that can hold it, so that value level
        // behavior can be checked through the only entry point there is.
        static bool parse_value(const char* value_text, ff_arena* arena, ff_value* value)
        {
            char json[512];
            ::_snprintf_s(json, sizeof(json), _TRUNCATE, "{ \"a\": %s }", value_text);

            ff_dict dict{};
            if (!ff_json_parse(sv(json), &dict, arena, nullptr))
            {
                return false;
            }

            *value = *ff_dict_get(&dict, sv("a"));
            return true;
        }

        static bool value_fails(const char* value_text, ff_arena* arena)
        {
            ff_value value{};
            return !parse_value(value_text, arena, &value);
        }

        static bool value_is_string(const char* value_text, ff_arena* arena, const char* expected)
        {
            ff_value value{};
            return parse_value(value_text, arena, &value) &&
                value.type == ff_value_type_string &&
                view_equals(ff_value_as_string(&value), expected);
        }

        TEST_METHOD(parse_rejects_invalid_utf8)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* error_pos = nullptr;

            Assert::IsFalse(ff_json_parse(sv("{ \"a\": \"\xFF\xFE\" }"), &dict, &arena, &error_pos));
            Assert::IsFalse(ff_json_parse(sv("{ \"a\": \"\xC0\xAF\" }"), &dict, &arena, nullptr));

            // A bad key is caught the same way a bad value is.
            Assert::IsFalse(ff_json_parse(sv("{ \"\xC0\xAF\": 1 }"), &dict, &arena, nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_numbers_that_overflow_to_infinity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // The number scans fine, but the value does not fit, and JSON cannot write infinity
            // back out, so it is refused rather than silently becoming inf.
            Assert::IsTrue(value_fails("1e400", &arena));
            Assert::IsTrue(value_fails("-1e400", &arena));

            // The largest double still works, so the check is not simply rejecting big numbers.
            ff_value value{};
            Assert::IsTrue(parse_value("1.7976931348623157e308", &arena, &value));
            Assert::IsTrue(ff_value_type_float64 == value.type);

            // Underflow is ordinary loss of precision, so it stays allowed.
            Assert::IsTrue(parse_value("1e-400", &arena, &value));
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(0.0, value.f64);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_bad_strings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            Assert::IsTrue(value_fails("\"unterminated", &arena));
            Assert::IsTrue(value_fails("\"bad \\q escape\"", &arena));
            Assert::IsTrue(value_fails("\"short \\u12\"", &arena));
            Assert::IsTrue(value_fails("\"bad \\uZZZZ hex\"", &arena));
            Assert::IsTrue(value_fails("\"raw \n newline\"", &arena));
            Assert::IsTrue(value_fails("\"trailing backslash\\", &arena));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_malformed_numbers)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_value value{};
            Assert::IsTrue(parse_value("0", &arena, &value));
            Assert::IsTrue(parse_value("-1", &arena, &value));
            Assert::IsTrue(parse_value("1.5", &arena, &value));
            Assert::IsTrue(parse_value("1e10", &arena, &value));
            Assert::IsTrue(parse_value("-1.5E-10", &arena, &value));

            // A number has to have digits everywhere the grammar requires them.
            Assert::IsTrue(value_fails("-", &arena));
            Assert::IsTrue(value_fails("1.", &arena));
            Assert::IsTrue(value_fails("1e", &arena));
            Assert::IsTrue(value_fails("1e+", &arena));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_leading_zeros)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Zero stands alone, and may still start a fraction or an exponent.
            ff_value value{};
            Assert::IsTrue(parse_value("0", &arena, &value));
            Assert::IsTrue(parse_value("-0", &arena, &value));
            Assert::IsTrue(parse_value("0.5", &arena, &value));
            Assert::IsTrue(parse_value("0e1", &arena, &value));

            // But a zero followed by another digit is not one number.
            Assert::IsTrue(value_fails("01", &arena));
            Assert::IsTrue(value_fails("-01", &arena));
            Assert::IsTrue(value_fails("00", &arena));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_accepts_valid_utf8_in_strings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Two, three and four byte sequences, and the highest code point there is.
            Assert::IsTrue(value_is_string("\"\xC3\xA9\"", &arena, "\xC3\xA9"));
            Assert::IsTrue(value_is_string("\"\xE2\x82\xAC\"", &arena, "\xE2\x82\xAC"));
            Assert::IsTrue(value_is_string("\"\xF0\x9F\x98\x80\"", &arena, "\xF0\x9F\x98\x80"));
            Assert::IsTrue(value_is_string("\"\xF4\x8F\xBF\xBF\"", &arena, "\xF4\x8F\xBF\xBF"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_invalid_utf8_in_strings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // A lone continuation byte, and lead bytes that never start anything.
            Assert::IsTrue(value_fails("\"\x80\"", &arena));
            Assert::IsTrue(value_fails("\"\xFF\"", &arena));
            Assert::IsTrue(value_fails("\"\xFE\"", &arena));

            // Truncated sequences, including one cut off by the end of the string.
            Assert::IsTrue(value_fails("\"\xC3\"", &arena));
            Assert::IsTrue(value_fails("\"\xE2\x82\"", &arena));
            Assert::IsTrue(value_fails("\"\xF0\x9F\x98\"", &arena));

            // Overlong encodings: these smuggle ASCII past anything that checks bytes naively.
            Assert::IsTrue(value_fails("\"\xC0\xAF\"", &arena));
            Assert::IsTrue(value_fails("\"\xC1\xBF\"", &arena));
            Assert::IsTrue(value_fails("\"\xE0\x80\xAF\"", &arena));
            Assert::IsTrue(value_fails("\"\xF0\x80\x80\xAF\"", &arena));

            // Surrogates are not encodable, and nothing exists past U+10FFFF.
            Assert::IsTrue(value_fails("\"\xED\xA0\x80\"", &arena));
            Assert::IsTrue(value_fails("\"\xF4\x90\x80\x80\"", &arena));
            Assert::IsTrue(value_fails("\"\xF5\x80\x80\x80\"", &arena));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_unescapes_strings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            Assert::IsTrue(value_is_string("\"plain\"", &arena, "plain"));
            Assert::IsTrue(value_is_string(
                "\"a\\\"b\\\\c\\/d\\be\\ff\\ng\\rh\\ti\"", &arena, "a\"b\\c/d\be\ff\ng\rh\ti"));
            Assert::IsTrue(value_is_string("\"\"", &arena, ""));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_encodes_unicode_escapes_as_utf8)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // One byte, two bytes and three bytes of UTF-8.
            Assert::IsTrue(value_is_string("\"\\u0041\"", &arena, "A"));
            Assert::IsTrue(value_is_string("\"\\u00E9\"", &arena, "\xC3\xA9"));
            Assert::IsTrue(value_is_string("\"\\u20AC\"", &arena, "\xE2\x82\xAC"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_joins_a_surrogate_pair)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // U+1F600, which only fits as a pair in JSON and as four bytes in UTF-8.
            Assert::IsTrue(value_is_string("\"\\uD83D\\uDE00\"", &arena, "\xF0\x9F\x98\x80"));

            // A high surrogate with no low one cannot be encoded, so it becomes U+FFFD.
            Assert::IsTrue(value_is_string("\"\\uD83D\"", &arena, "\xEF\xBF\xBD"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_prefers_the_narrowest_number_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_value value{};
            Assert::IsTrue(parse_value("42", &arena, &value));
            Assert::IsTrue(ff_value_type_int32 == value.type);
            Assert::AreEqual(42, value.i32);

            // Past int32, but still exact as an integer.
            Assert::IsTrue(parse_value("5000000000", &arena, &value));
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual((int64_t)5000000000LL, value.i64);

            // Anything with a fraction or exponent is a double.
            Assert::IsTrue(parse_value("1.5", &arena, &value));
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(1.5, value.f64);

            Assert::IsTrue(parse_value("1e2", &arena, &value));
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(100.0, value.f64);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_keeps_every_digit_of_a_large_integer)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Going through a double would round this, so it is read as an integer directly.
            ff_value value{};
            Assert::IsTrue(parse_value("9223372036854775807", &arena, &value));
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual(INT64_MAX, value.i64);

            // INT64_MIN has no positive counterpart, so it is the one that negation can get wrong.
            Assert::IsTrue(parse_value("-9223372036854775808", &arena, &value));
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual(INT64_MIN, value.i64);

            // Past what an integer holds, so it falls back to a double.
            Assert::IsTrue(parse_value("99999999999999999999", &arena, &value));
            Assert::IsTrue(ff_value_type_float64 == value.type);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_an_unterminated_block_comment)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsFalse(ff_json_parse(sv("{ \"a\": 1 } /* never ends"), &dict, &arena, nullptr));
            Assert::IsFalse(ff_json_parse(sv("/* never ends"), &dict, &arena, nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_an_empty_object)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* error_pos = nullptr;

            Assert::IsTrue(ff_json_parse(sv("{}"), &dict, &arena, &error_pos));
            Assert::IsNull(error_pos);
            Assert::AreEqual((size_t)0, dict.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_every_scalar_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json =
                "{ \"b\": true, \"n\": null, \"i\": 7, \"d\": 2.5, \"s\": \"hi\" }";

            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));
            Assert::AreEqual((size_t)5, dict.count);

            Assert::IsTrue(ff_dict_get(&dict, sv("b"))->b);
            Assert::IsTrue(ff_value_type_null == ff_dict_get(&dict, sv("n"))->type);
            Assert::AreEqual(7, ff_dict_get(&dict, sv("i"))->i32);
            Assert::AreEqual(2.5, ff_dict_get(&dict, sv("d"))->f64);
            Assert::IsTrue(view_equals(ff_value_as_string(ff_dict_get(&dict, sv("s"))), "hi"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_a_nested_object)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json = "{ \"outer\": { \"inner\": { \"value\": 3 } } }";

            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));

            ff_value* outer = ff_dict_get(&dict, sv("outer"));
            Assert::IsTrue(ff_value_type_dict == outer->type);

            ff_dict* outer_dict = ff_value_as_dict(outer);
            ff_value* inner = ff_dict_get(outer_dict, sv("inner"));
            Assert::IsTrue(ff_value_type_dict == inner->type);

            ff_dict* inner_dict = ff_value_as_dict(inner);
            Assert::AreEqual(3, ff_dict_get(inner_dict, sv("value"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_arrays)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json = "{ \"empty\": [], \"nums\": [1, 2, 3], \"mixed\": [true, \"x\", 1.5, null] }";

            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));

            ff_value_span empty = ff_value_as_array(ff_dict_get(&dict, sv("empty")));
            Assert::AreEqual((size_t)0, empty.count);

            ff_value_span nums = ff_value_as_array(ff_dict_get(&dict, sv("nums")));
            Assert::AreEqual((size_t)3, nums.count);
            Assert::AreEqual(1, nums.data[0].i32);
            Assert::AreEqual(2, nums.data[1].i32);
            Assert::AreEqual(3, nums.data[2].i32);

            ff_value_span mixed = ff_value_as_array(ff_dict_get(&dict, sv("mixed")));
            Assert::AreEqual((size_t)4, mixed.count);
            Assert::IsTrue(mixed.data[0].b);
            Assert::IsTrue(view_equals(ff_value_as_string(&mixed.data[1]), "x"));
            Assert::AreEqual(1.5, mixed.data[2].f64);
            Assert::IsTrue(ff_value_type_null == mixed.data[3].type);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_objects_inside_arrays)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json = "{ \"items\": [ { \"id\": 1 }, { \"id\": 2 } ] }";

            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));

            ff_value_span items = ff_value_as_array(ff_dict_get(&dict, sv("items")));
            Assert::AreEqual((size_t)2, items.count);

            Assert::AreEqual(1, ff_dict_get(ff_value_as_dict(&items.data[0]), sv("id"))->i32);
            Assert::AreEqual(2, ff_dict_get(ff_value_as_dict(&items.data[1]), sv("id"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reads_deeply_nested_arrays)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsTrue(ff_json_parse(sv("{ \"a\": [[[1]]] }"), &dict, &arena, nullptr));

            ff_value_span level1 = ff_value_as_array(ff_dict_get(&dict, sv("a")));
            ff_value_span level2 = ff_value_as_array(&level1.data[0]);
            ff_value_span level3 = ff_value_as_array(&level2.data[0]);

            Assert::AreEqual((size_t)1, level3.count);
            Assert::AreEqual(1, level3.data[0].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parsed_strings_survive_the_source_text)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            char json[] = "{ \"key\": \"value\" }";
            ff_dict dict{};
            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));

            // Strings are copied into the arena during the parse, so scribbling on the source now
            // must not change what was parsed.
            ::memset(json, 'x', sizeof(json) - 1);

            Assert::IsTrue(view_equals(ff_value_as_string(ff_dict_get(&dict, sv("key"))), "value"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_ignores_comments_and_whitespace)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json =
                "// header\r\n"
                "{\r\n"
                "  /* the answer */ \"a\": 1,\r\n"
                "  \"b\": 2 // trailing\r\n"
                "}\r\n";

            Assert::IsTrue(ff_json_parse(sv(json), &dict, &arena, nullptr));
            Assert::AreEqual(1, ff_dict_get(&dict, sv("a"))->i32);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("b"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_uses_the_first_value_for_a_repeated_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsTrue(ff_json_parse(sv("{ \"a\": 1, \"a\": 2 }"), &dict, &arena, nullptr));

            // Both entries are kept, and the lookup finds the one that was parsed first. This is
            // deliberately not the JSON convention of the last value winning.
            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(1, ff_dict_get(&dict, sv("a"))->i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_reports_the_position_of_an_error)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            const char* json = "{ \"a\": 1, \"b\": @ }";
            const char* error_pos = nullptr;

            Assert::IsFalse(ff_json_parse(sv(json), &dict, &arena, &error_pos));
            Assert::IsNotNull(error_pos);
            Assert::AreEqual('@', *error_pos);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_malformed_documents)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const char* bad[] =
            {
                "",                      // nothing at all
                "[]",                    // the root has to be an object
                "42",                    // ditto
                "{",                     // never closed
                "{ \"a\" }",             // no colon or value
                "{ \"a\": }",            // no value
                "{ \"a\": 1 ",           // never closed
                "{ \"a\": 1, }",         // trailing comma
                "{ \"a\": [1, ] }",      // trailing comma in an array
                "{ \"a\": [1 2] }",      // missing comma
                "{ a: 1 }",              // unquoted key
                "{ \"a\": 1 } extra",    // junk after the root
                "{ \"a\": 1 }{}",        // a second document
                "{ \"a\": 01 }",         // not a valid number, and junk follows
            };

            for (const char* json : bad)
            {
                ff_dict dict{};
                const char* error_pos = nullptr;

                Assert::IsFalse(ff_json_parse(sv(json), &dict, &arena, &error_pos));
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_handles_deeply_nested_arrays)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // There is no depth limit, so this has to parse rather than be refused. Kept to a
            // depth the default stack handles comfortably, since each level is one recursion.
            static const size_t nest = 256;
            static char json[nest * 2 + 16]{};
            size_t count = 0;

            const char* prefix = "{ \"a\":";
            for (const char* p = prefix; *p; p++)
            {
                json[count++] = *p;
            }

            for (size_t i = 0; i < nest; i++)
            {
                json[count++] = '[';
            }

            for (size_t i = 0; i < nest; i++)
            {
                json[count++] = ']';
            }

            json[count++] = ' ';
            json[count++] = '}';

            ff_dict dict{};
            ff_string_view view{ json, count };

            Assert::IsTrue(ff_json_parse(view, &dict, &arena, nullptr));

            // Walk back down to prove every level really is there.
            const ff_value* value = ff_dict_get(&dict, FF_SVL("a"));
            for (size_t i = 0; i < nest; i++)
            {
                Assert::IsNotNull(value);
                Assert::IsTrue(ff_value_type_array == value->type);

                ff_value_span items = ff_value_as_array(value);
                value = (i + 1 < nest) ? &items.data[0] : nullptr;

                if (i + 1 < nest)
                {
                    Assert::AreEqual(size_t(1), items.count);
                }
                else
                {
                    Assert::AreEqual(size_t(0), items.count);
                }
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_accepts_a_null_error_pos)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsFalse(ff_json_parse(sv("{ bad }"), &dict, &arena, nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_reads_back_every_value_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            Assert::IsTrue(ff_json_parse_idict(sv(
                "{ \"num\": 7, \"big\": 5000000000, \"real\": 1.5, \"yes\": true, \"no\": false,"
                "  \"nil\": null, \"text\": \"hello\", \"list\": [1, 2, 3],"
                "  \"nested\": { \"inner\": \"deep\" } }"),
                &dict, &arena, nullptr));

            Assert::AreEqual(7, ff_idict_get(&dict, sv("num"))->i32);
            Assert::AreEqual((int64_t)5000000000, ff_idict_get(&dict, sv("big"))->i64);
            Assert::AreEqual(1.5, ff_idict_get(&dict, sv("real"))->f64);
            Assert::IsTrue(ff_idict_get(&dict, sv("yes"))->b);
            Assert::IsFalse(ff_idict_get(&dict, sv("no"))->b);
            Assert::IsTrue(ff_value_type_null == ff_idict_get(&dict, sv("nil"))->type);

            const ff_ivalue* text = ff_idict_get(&dict, sv("text"));
            Assert::IsTrue(view_equals(ff_ivalue_as_string(text, &dict), "hello"));

            ff_ivalue_span list = ff_ivalue_as_array(ff_idict_get(&dict, sv("list")), &dict);
            Assert::AreEqual(size_t(3), list.count);
            Assert::AreEqual(1, list.data[0].i32);
            Assert::AreEqual(3, list.data[2].i32);

            ff_idict nested = ff_ivalue_as_dict(ff_idict_get(&dict, sv("nested")), &dict);
            Assert::IsTrue(view_equals(ff_ivalue_as_string(ff_idict_get(&nested, sv("inner")), &nested), "deep"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_outlives_the_parsed_text)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Build the text on the stack, then scribble over it. Every string must have been
            // copied into the block, so nothing may still point at the original bytes.
            char text[64]{};
            ::strcpy_s(text, sizeof(text), "{ \"key\": \"value\" }");

            ff_idict dict{};
            ff_string_view view{ text, ::strlen(text) };
            Assert::IsTrue(ff_json_parse_idict(view, &dict, &arena, nullptr));

            ::memset(text, '?', sizeof(text));

            const ff_ivalue* value = ff_idict_get(&dict, sv("key"));
            Assert::IsTrue(view_equals(ff_ivalue_as_string(value, &dict), "value"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_result_can_be_saved_and_loaded)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            Assert::IsTrue(ff_json_parse_idict(sv("{ \"a\": \"x\", \"b\": [1, 2] }"), &dict, &arena, nullptr));

            // A block that survives a save and a validating load is genuinely self contained,
            // which is the whole point of throwing the scratch arena away.
            ff_span saved = ff_idict_save(&dict, &arena);

            ff_idict loaded{};
            Assert::IsTrue(ff_idict_load(&loaded, saved, true, true));

            Assert::IsTrue(view_equals(ff_ivalue_as_string(ff_idict_get(&loaded, sv("a")), &loaded), "x"));

            ff_ivalue_span items = ff_ivalue_as_array(ff_idict_get(&loaded, sv("b")), &loaded);
            Assert::AreEqual(size_t(2), items.count);
            Assert::AreEqual(2, items.data[1].i32);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_reports_errors_like_the_dict_version)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            const char* json = "{ \"a\": nope }";
            const char* error_pos = nullptr;

            ff_idict dict{};
            Assert::IsFalse(ff_json_parse_idict(sv(json), &dict, &arena, &error_pos));
            Assert::IsNotNull(error_pos);
            Assert::AreEqual(ptrdiff_t(7), error_pos - json);

            // A null error_pos is still allowed.
            Assert::IsFalse(ff_json_parse_idict(sv("{ bad }"), &dict, &arena, nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_handles_an_empty_object)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            Assert::IsTrue(ff_json_parse_idict(sv("{}"), &dict, &arena, nullptr));
            Assert::IsNull(ff_idict_get(&dict, sv("nothing")));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_idict_keeps_the_first_value_for_a_repeated_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            Assert::IsTrue(ff_json_parse_idict(sv("{ \"a\": 1, \"a\": 2 }"), &dict, &arena, nullptr));
            Assert::AreEqual(1, ff_idict_get(&dict, sv("a"))->i32);

            ff_arena_destroy(&arena);
        }
    };
}
