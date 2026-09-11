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

        // Tokenize everything and return the types, so a test can state the shape it expects.
        static size_t collect_types(const char* text, ff_json_token_type* types, size_t max_types)
        {
            ff_json_tokenizer tokenizer{};
            ff_json_tokenizer_init(&tokenizer, sv(text));
            size_t count = 0;

            while (count < max_types)
            {
                ff_json_token token = ff_json_tokenizer_next(&tokenizer);
                types[count++] = token.type;

                if (token.type == ff_json_token_type_none || token.type == ff_json_token_type_error)
                {
                    break;
                }
            }

            return count;
        }

        static ff_json_token single_token(const char* text)
        {
            ff_json_tokenizer tokenizer{};
            ff_json_tokenizer_init(&tokenizer, sv(text));
            return ff_json_tokenizer_next(&tokenizer);
        }

        TEST_METHOD(tokenizer_returns_none_for_empty_text)
        {
            Assert::IsTrue(ff_json_token_type_none == single_token("").type);
            Assert::IsTrue(ff_json_token_type_none == single_token("   \t\r\n  ").type);
        }

        TEST_METHOD(tokenizer_reads_punctuation)
        {
            ff_json_token_type types[16]{};
            Assert::AreEqual((size_t)7, collect_types("{}[],:", types, 16));

            Assert::IsTrue(ff_json_token_type_open_curly == types[0]);
            Assert::IsTrue(ff_json_token_type_close_curly == types[1]);
            Assert::IsTrue(ff_json_token_type_open_bracket == types[2]);
            Assert::IsTrue(ff_json_token_type_close_bracket == types[3]);
            Assert::IsTrue(ff_json_token_type_comma == types[4]);
            Assert::IsTrue(ff_json_token_type_colon == types[5]);
            Assert::IsTrue(ff_json_token_type_none == types[6]);
        }

        TEST_METHOD(tokenizer_reads_keywords)
        {
            Assert::IsTrue(ff_json_token_type_true == single_token("true").type);
            Assert::IsTrue(ff_json_token_type_false == single_token("false").type);
            Assert::IsTrue(ff_json_token_type_null == single_token("null").type);

            // A prefix of a keyword is not the keyword, and neither is a longer word.
            Assert::IsTrue(ff_json_token_type_error == single_token("tru").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("truex").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("nul").type);
        }

        TEST_METHOD(token_text_points_into_the_source)
        {
            const char* text = "   \"abc\"   ";
            ff_json_tokenizer tokenizer{};
            ff_json_tokenizer_init(&tokenizer, sv(text));

            ff_json_token token = ff_json_tokenizer_next(&tokenizer);

            // The token is a slice of the original buffer, not a copy, and it keeps its quotes.
            Assert::IsTrue(token.text.data == text + 3);
            Assert::AreEqual((size_t)5, token.text.count);
            Assert::IsTrue(view_equals(token.text, "\"abc\""));
        }

        TEST_METHOD(tokenizer_rejects_bad_strings)
        {
            Assert::IsTrue(ff_json_token_type_error == single_token("\"unterminated").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"bad \\q escape\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"short \\u12\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"bad \\uZZZZ hex\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"raw \n newline\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"trailing backslash\\").type);
        }

        TEST_METHOD(tokenizer_reads_numbers)
        {
            Assert::IsTrue(ff_json_token_type_number == single_token("0").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("-1").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("1.5").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("1e10").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("-1.5E-10").type);

            // A number has to have digits everywhere the grammar requires them.
            Assert::IsTrue(ff_json_token_type_error == single_token("-").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("1.").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("1e").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("1e+").type);
        }

        TEST_METHOD(tokenizer_rejects_leading_zeros)
        {
            // Zero stands alone, and may still start a fraction or an exponent.
            Assert::IsTrue(ff_json_token_type_number == single_token("0").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("-0").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("0.5").type);
            Assert::IsTrue(ff_json_token_type_number == single_token("0e1").type);

            // But a zero followed by another digit is not one number.
            Assert::IsTrue(ff_json_token_type_error == single_token("01").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("-01").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("00").type);
        }

        TEST_METHOD(tokenizer_accepts_valid_utf8_in_strings)
        {
            // Two, three and four byte sequences, and the highest code point there is.
            Assert::IsTrue(ff_json_token_type_string == single_token("\"\xC3\xA9\"").type);
            Assert::IsTrue(ff_json_token_type_string == single_token("\"\xE2\x82\xAC\"").type);
            Assert::IsTrue(ff_json_token_type_string == single_token("\"\xF0\x9F\x98\x80\"").type);
            Assert::IsTrue(ff_json_token_type_string == single_token("\"\xF4\x8F\xBF\xBF\"").type);
        }

        TEST_METHOD(tokenizer_rejects_invalid_utf8_in_strings)
        {
            // A lone continuation byte, and lead bytes that never start anything.
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\x80\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xFF\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xFE\"").type);

            // Truncated sequences, including one cut off by the end of the text.
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xC3\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xE2\x82\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xF0\x9F\x98\"").type);

            // Overlong encodings: these smuggle ASCII past anything that checks bytes naively.
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xC0\xAF\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xC1\xBF\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xE0\x80\xAF\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xF0\x80\x80\xAF\"").type);

            // Surrogates are not encodable, and nothing exists past U+10FFFF.
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xED\xA0\x80\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xF4\x90\x80\x80\"").type);
            Assert::IsTrue(ff_json_token_type_error == single_token("\"\xF5\x80\x80\x80\"").type);
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

        TEST_METHOD(token_value_rejects_numbers_that_overflow_to_infinity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // The token scans fine, but the value does not fit, and JSON cannot write infinity
            // back out, so it is refused rather than silently becoming inf.
            ff_json_token token = single_token("1e400");
            Assert::IsTrue(ff_json_token_type_number == token.type);
            Assert::IsTrue(ff_value_type_empty == ff_json_token_value(&token, &arena).type);

            token = single_token("-1e400");
            Assert::IsTrue(ff_value_type_empty == ff_json_token_value(&token, &arena).type);

            // The largest double still works, so the check is not simply rejecting big numbers.
            token = single_token("1.7976931348623157e308");
            Assert::IsTrue(ff_value_type_float64 == ff_json_token_value(&token, &arena).type);

            // Underflow is ordinary loss of precision, so it stays allowed.
            token = single_token("1e-400");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(0.0, value.f64);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(parse_rejects_numbers_that_overflow_to_infinity)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsFalse(ff_json_parse(sv("{ \"a\": 1e400 }"), &dict, &arena, nullptr));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(tokenizer_skips_comments)
        {
            ff_json_token_type types[16]{};
            Assert::AreEqual((size_t)3, collect_types("// line\n { /* block */ } // trailing", types, 16));

            Assert::IsTrue(ff_json_token_type_open_curly == types[0]);
            Assert::IsTrue(ff_json_token_type_close_curly == types[1]);
            Assert::IsTrue(ff_json_token_type_none == types[2]);
        }

        TEST_METHOD(tokenizer_rejects_an_unterminated_block_comment)
        {
            // The '/' is put back and tokenized, so this reports an error instead of end of text.
            Assert::IsTrue(ff_json_token_type_error == single_token("/* never ends").type);
        }

        TEST_METHOD(tokenizer_always_advances_past_an_error)
        {
            // An error token that consumed nothing would make any caller loop forever.
            ff_json_tokenizer tokenizer{};
            ff_json_tokenizer_init(&tokenizer, sv("@@"));

            ff_json_token first = ff_json_tokenizer_next(&tokenizer);
            Assert::IsTrue(ff_json_token_type_error == first.type);
            Assert::AreEqual((size_t)1, first.text.count);

            ff_json_token second = ff_json_tokenizer_next(&tokenizer);
            Assert::IsTrue(ff_json_token_type_error == second.type);
            Assert::IsTrue(second.text.data > first.text.data);
        }

        TEST_METHOD(token_value_converts_literals)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_json_token token = single_token("true");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_boolean == value.type);
            Assert::IsTrue(value.b);

            token = single_token("false");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_boolean == value.type);
            Assert::IsFalse(value.b);

            token = single_token("null");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_null == value.type);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_prefers_the_narrowest_number_type)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_json_token token = single_token("42");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_int32 == value.type);
            Assert::AreEqual(42, value.i32);

            // Past int32, but still exact as an integer.
            token = single_token("5000000000");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual((int64_t)5000000000LL, value.i64);

            // Anything with a fraction or exponent is a double.
            token = single_token("1.5");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(1.5, value.f64);

            token = single_token("1e2");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_float64 == value.type);
            Assert::AreEqual(100.0, value.f64);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_keeps_every_digit_of_a_large_integer)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // Going through a double would round this, so it is read as an integer directly.
            ff_json_token token = single_token("9223372036854775807");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual(INT64_MAX, value.i64);

            // INT64_MIN has no positive counterpart, so it is the one that negation can get wrong.
            token = single_token("-9223372036854775808");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_int64 == value.type);
            Assert::AreEqual(INT64_MIN, value.i64);

            // Past what an integer holds, so it falls back to a double.
            token = single_token("99999999999999999999");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_float64 == value.type);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_unescapes_strings)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_json_token token = single_token("\"plain\"");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_string == value.type);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "plain"));

            token = single_token("\"a\\\"b\\\\c\\/d\\be\\ff\\ng\\rh\\ti\"");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "a\"b\\c/d\be\ff\ng\rh\ti"));

            token = single_token("\"\"");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(ff_value_type_string == value.type);
            Assert::AreEqual((size_t)0, ff_value_as_string(&value).count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_encodes_unicode_escapes_as_utf8)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // One byte, two bytes and three bytes of UTF-8.
            ff_json_token token = single_token("\"\\u0041\"");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "A"));

            token = single_token("\"\\u00E9\"");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "\xC3\xA9"));

            token = single_token("\"\\u20AC\"");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "\xE2\x82\xAC"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_joins_a_surrogate_pair)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            // U+1F600, which only fits as a pair in JSON and as four bytes in UTF-8.
            ff_json_token token = single_token("\"\\uD83D\\uDE00\"");
            ff_value value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "\xF0\x9F\x98\x80"));

            // A high surrogate with no low one cannot be encoded, so it becomes U+FFFD.
            token = single_token("\"\\uD83D\"");
            value = ff_json_token_value(&token, &arena);
            Assert::IsTrue(view_equals(ff_value_as_string(&value), "\xEF\xBF\xBD"));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(token_value_is_empty_for_structural_tokens)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_json_token token = single_token("{");
            Assert::IsTrue(ff_value_type_empty == ff_json_token_value(&token, &arena).type);

            token = single_token("@");
            Assert::IsTrue(ff_value_type_empty == ff_json_token_value(&token, &arena).type);

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

        TEST_METHOD(parse_uses_the_last_value_for_a_repeated_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_dict dict{};
            Assert::IsTrue(ff_json_parse(sv("{ \"a\": 1, \"a\": 2 }"), &dict, &arena, nullptr));

            // Both entries are kept, and the lookup finds the one that was parsed last.
            Assert::AreEqual((size_t)2, dict.count);
            Assert::AreEqual(2, ff_dict_get(&dict, sv("a"))->i32);

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

        TEST_METHOD(parse_idict_keeps_the_last_value_for_a_repeated_key)
        {
            ff_arena arena{};
            ff_arena_init_heap_global(&arena, 4096);

            ff_idict dict{};
            Assert::IsTrue(ff_json_parse_idict(sv("{ \"a\": 1, \"a\": 2 }"), &dict, &arena, nullptr));
            Assert::AreEqual(2, ff_idict_get(&dict, sv("a"))->i32);

            ff_arena_destroy(&arena);
        }
    };
}
