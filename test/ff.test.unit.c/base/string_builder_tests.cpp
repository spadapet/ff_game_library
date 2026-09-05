#include "pch.h"

// Compare a string_builder's view to a null-terminated expected string.
static bool view_equals(ff_string_view view, const char* expected)
{
    size_t expected_len = ::strlen(expected);
    if (view.count != expected_len)
    {
        return false;
    }

    return ::memcmp(view.data, expected, expected_len) == 0;
}

// Null-terminate the builder's content in place (without counting the '\0') and return it as a
// C-string. Mirrors how callers terminate before reading data now that string_builder keeps
// no automatic terminator.
static const char* sb_cstr(ff_string_builder& sb)
{
    ff_string_builder_reserve(&sb, sb.count + 1);
    sb.data[sb.count] = '\0';
    return sb.data;
}

// Forwarder that builds a va_list and calls the *_v entry point (simulating a caller that already
// has a va_list, e.g. a variadic wrapper forwarding its arguments).
static void call_append_format_v(ff_string_builder* sb, ff_string_view format, ...)
{
    va_list args;
    va_start(args, format);
    ff_string_builder_append_format_v(sb, format, args);
    va_end(args);
}

namespace ff::test::base
{
    TEST_CLASS(string_builder_tests)
    {
    public:
        // ====================================================================
        // Initialization
        // ====================================================================
        TEST_METHOD(init_default)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            Assert::AreEqual((void*)&arena, (void*)sb.arena);
            Assert::IsNotNull(sb.data);
            Assert::AreEqual((size_t)0, sb.count);
            Assert::IsTrue(sb.capacity > 0);
            Assert::AreEqual("", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_custom_capacity)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 8192);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 4096);

            Assert::IsTrue(sb.capacity >= 4096);
            Assert::AreEqual((size_t)0, sb.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_with_string_view_seed)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init_string(&sb, &arena, ff_string_view{ "hello", 5 });

            Assert::AreEqual((size_t)5, sb.count);
            Assert::AreEqual("hello", sb_cstr(sb));

            // Can keep appending after a seeded init.
            ff_string_builder_append(&sb, FF_SVL(" world"));
            Assert::AreEqual("hello world", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_with_empty_string_view)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init_string(&sb, &arena, ff_string_view{ nullptr, 0 });

            Assert::AreEqual((size_t)0, sb.count);
            Assert::AreEqual("", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(init_seed_larger_than_default_capacity)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 8192);

            // Seed bigger than the default initial capacity must still fit in one buffer.
            char big[2000];
            for (int i = 0; i < 2000; ++i)
            {
                big[i] = (char)('a' + (i % 26));
            }

            ff_string_builder sb;
            ff_string_builder_init_string(&sb, &arena, ff_string_view{ big, sizeof(big) });

            Assert::AreEqual((size_t)2000, sb.count);
            Assert::IsTrue(::memcmp(sb.data, big, sizeof(big)) == 0);

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Append
        // ====================================================================
        TEST_METHOD(append_char)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append_char(&sb, 'a');
            ff_string_builder_append_char(&sb, 'b');
            ff_string_builder_append_char(&sb, 'c');

            Assert::AreEqual((size_t)3, sb.count);
            Assert::AreEqual("abc", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_cstr)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append(&sb, FF_SVL("hello"));
            ff_string_builder_append(&sb, FF_SVL(", "));
            ff_string_builder_append(&sb, FF_SVL("world"));

            Assert::AreEqual("hello, world", sb_cstr(sb));
            Assert::AreEqual((size_t)12, sb.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_string_view)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_view first{ "foo", 3 };
            ff_string_view second{ "bar", 3 };
            ff_string_builder_append(&sb, first);
            ff_string_builder_append(&sb, second);

            Assert::AreEqual("foobar", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_partial_string_view)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            const char* text = "abcdefgh";
            ff_string_builder_append(&sb, ff_string_view{ text, 3 }); // "abc"
            ff_string_builder_append(&sb, ff_string_view{ text + 5, 3 }); // "fgh"

            Assert::AreEqual("abcfgh", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_null_cstr_is_noop)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append(&sb, FF_SVL("keep"));
            ff_string_builder_append(&sb, ff_sz_view((const char*)nullptr));

            Assert::AreEqual("keep", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_empty_is_noop)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append(&sb, FF_SVL("keep"));
            ff_string_builder_append(&sb, ff_string_view{ "", 0 });

            Assert::AreEqual((size_t)4, sb.count);
            Assert::AreEqual("keep", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // view() / manual null-termination
        // ====================================================================
        TEST_METHOD(view_returns_start_size)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("abcdef"));

            ff_string_view view = ff_string_builder_view(&sb);
            Assert::AreEqual((size_t)6, view.count);
            Assert::IsTrue(::view_equals(view, "abcdef"));
            Assert::AreEqual((void*)sb.data, (void*)view.data);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(manual_terminator_does_not_change_count)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("data"));

            const char* result = sb_cstr(sb);
            Assert::AreEqual('\0', result[4]);
            Assert::AreEqual((size_t)4, sb.count); // count excludes the manual terminator

            // Appending after writing a terminator continues correctly.
            ff_string_builder_append_char(&sb, '!');
            Assert::AreEqual("data!", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Insert
        // ====================================================================
        TEST_METHOD(insert_at_front)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("world"));

            ff_string_builder_insert(&sb, 0, FF_SVL("hello "));

            Assert::AreEqual("hello world", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(insert_in_middle)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("helloworld"));

            ff_string_builder_insert(&sb, 5, FF_SVL(", "));

            Assert::AreEqual("hello, world", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(insert_at_end)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("hello"));

            ff_string_builder_insert(&sb, sb.count, FF_SVL("!!!"));

            Assert::AreEqual("hello!!!", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(insert_char)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("ac"));

            ff_string_builder_insert_char(&sb, 1, 'b');

            Assert::AreEqual("abc", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(insert_string_view)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("AC"));

            ff_string_view middle{ "B", 1 };
            ff_string_builder_insert(&sb, 1, middle);

            Assert::AreEqual("ABC", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Remove
        // ====================================================================
        TEST_METHOD(remove_from_middle)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("hello, world"));

            ff_string_builder_remove(&sb, 5, 2); // remove ", "

            Assert::AreEqual("helloworld", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(remove_from_front)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("XXabc"));

            ff_string_builder_remove(&sb, 0, 2);

            Assert::AreEqual("abc", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(remove_count_clamped_to_end)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("abcdef"));

            ff_string_builder_remove(&sb, 3, 100); // count past end => trims to end

            Assert::AreEqual("abc", sb_cstr(sb));
            Assert::AreEqual((size_t)3, sb.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(remove_zero_is_noop)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("abc"));

            ff_string_builder_remove(&sb, 1, 0);

            Assert::AreEqual("abc", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Reset
        // ====================================================================
        TEST_METHOD(reset_clears_but_keeps_buffer)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("some content"));

            char* data_before = sb.data;
            size_t capacity_before = sb.capacity;

            ff_string_builder_reset(&sb);

            Assert::AreEqual((size_t)0, sb.count);
            Assert::AreEqual("", sb_cstr(sb));
            Assert::AreEqual((void*)data_before, (void*)sb.data); // same buffer reused
            Assert::AreEqual(capacity_before, sb.capacity);

            ff_string_builder_append(&sb, FF_SVL("reused"));
            Assert::AreEqual("reused", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // reserve / growth
        // ====================================================================
        TEST_METHOD(reserve_grows_capacity)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 64);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 16);

            ff_string_builder_reserve(&sb, 10000);
            Assert::IsTrue(sb.capacity >= 10000);
            Assert::AreEqual((size_t)0, sb.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(growth_doubles_and_preserves_content)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 64);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 16);

            size_t initial_capacity = sb.capacity;

            // Append enough to force several doublings.
            for (int i = 0; i < 1000; ++i)
            {
                ff_string_builder_append_char(&sb, 'x');
            }

            Assert::AreEqual((size_t)1000, sb.count);
            Assert::IsTrue(sb.capacity > initial_capacity);

            // Every char should still be 'x', and a manual terminator yields a valid C-string.
            const char* result = sb_cstr(sb);
            for (int i = 0; i < 1000; ++i)
            {
                Assert::AreEqual('x', result[i]);
            }
            Assert::AreEqual('\0', result[1000]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(growth_preserves_content_arena)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 64);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 8); // small so growth via arena realloc happens

            for (int i = 0; i < 500; ++i)
            {
                ff_string_builder_append_char(&sb, (char)('a' + (i % 26)));
            }

            Assert::AreEqual((size_t)500, sb.count);

            const char* result = sb_cstr(sb);
            for (int i = 0; i < 500; ++i)
            {
                Assert::AreEqual((char)('a' + (i % 26)), result[i]);
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(arena_relocation_on_interleaved_alloc)
        {
            // When another arena allocation happens between appends, the builder is no longer the
            // arena's last allocation, so the next grow relocates the buffer. Content must survive.
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 8);
            ff_string_builder_append(&sb, FF_SVL("12345678")); // fill to initial capacity to force a grow next time

            // Interleave an unrelated arena allocation.
            void* other = ff_arena_alloc(&arena, 64, 8);
            Assert::IsNotNull(other);

            // This append forces a grow; since 'other' is now last, the builder relocates.
            ff_string_builder_append(&sb, FF_SVL("ABCDEFGH"));

            Assert::AreEqual("12345678ABCDEFGH", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Combined / round-trip
        // ====================================================================
        TEST_METHOD(append_insert_remove_round_trip)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append(&sb, FF_SVL("The quick fox"));
            ff_string_builder_insert(&sb, 10, FF_SVL("brown ")); // "The quick brown fox"
            Assert::AreEqual("The quick brown fox", sb_cstr(sb));

            ff_string_builder_remove(&sb, 3, 6); // remove " quick" => "The brown fox"
            Assert::AreEqual("The brown fox", sb_cstr(sb));

            ff_string_builder_append_char(&sb, '!');
            Assert::AreEqual("The brown fox!", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Formatted append / init
        // ====================================================================
        TEST_METHOD(append_format_basic)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append_format(&sb, FF_SVL("%s = %d"), "answer", 42);

            Assert::AreEqual("answer = 42", sb_cstr(sb));
            Assert::AreEqual((size_t)11, sb.count);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_after_existing_content)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("prefix:"));

            ff_string_builder_append_format(&sb, FF_SVL(" %d-%d"), 1, 2);

            Assert::AreEqual("prefix: 1-2", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_chains)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append_format(&sb, FF_SVL("[%d]"), 1);
            ff_string_builder_append_format(&sb, FF_SVL("[%d]"), 2);
            ff_string_builder_append_format(&sb, FF_SVL("[%d]"), 3);

            Assert::AreEqual("[1][2][3]", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_percent_literal)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_builder_append_format(&sb, FF_SVL("%d%% done"), 50);

            Assert::AreEqual("50% done", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_grows_buffer)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 64);

            ff_string_builder sb;
            ff_string_builder_init_capacity(&sb, &arena, 8); // small so the formatted string forces growth

            // Produces a long string (well past the initial capacity).
            ff_string_builder_append_format(&sb, FF_SVL("%0500d"), 7);

            Assert::AreEqual((size_t)500, sb.count);
            const char* result = sb_cstr(sb);
            Assert::AreEqual('\0', result[500]);
            // Last char is the '7', the rest are leading zeros.
            Assert::AreEqual('7', result[499]);
            Assert::AreEqual('0', result[0]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_large_format_string_spills_to_heap)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            // Build a format string longer than append_format_v's 1024-byte stack buffer so the
            // temp arena has to spill the null-terminated copy onto the heap. It's 2000 literal
            // 'x' chars followed by "%d".
            ff_string_builder format;
            ff_string_builder_init(&format, &arena);
            for (int i = 0; i < 2000; ++i)
            {
                ff_string_builder_append_char(&format, 'x');
            }
            ff_string_builder_append(&format, FF_SVL("%d"));

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append_format(&sb, ff_string_builder_view(&format), 7);

            Assert::AreEqual((size_t)2001, sb.count); // 2000 'x' + "7"
            const char* result = sb_cstr(sb);
            for (int i = 0; i < 2000; ++i)
            {
                Assert::AreEqual('x', result[i]);
            }
            Assert::AreEqual('7', result[2000]);
            Assert::AreEqual('\0', result[2001]);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(append_format_v_appends)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("x="));
            call_append_format_v(&sb, FF_SVL("%d"), 5);

            Assert::AreEqual("x=5", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        // ====================================================================
        // Copy (persistent copy)
        // ====================================================================
        TEST_METHOD(copy_returns_independent_null_terminated_copy)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("hello"));

            ff_string_view copied = ff_string_builder_copy(&sb);
            Assert::IsNotNull(copied.data);
            Assert::IsTrue(::view_equals(copied, "hello")); // view content
            Assert::AreEqual((size_t)5, copied.count); // count excludes the terminator
            Assert::AreEqual('\0', copied.data[copied.count]); // null-terminated past the count
            Assert::IsTrue(copied.data != sb.data); // a separate allocation, not the builder's working buffer

            // Reusing the builder must not disturb the copied string.
            ff_string_builder_reset(&sb);
            ff_string_builder_append(&sb, FF_SVL("world"));
            Assert::IsTrue(::view_equals(copied, "hello"));
            Assert::AreEqual("world", sb_cstr(sb));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(copy_into_separate_arena_outlives_builder_arena)
        {
            ff_arena dest;
            ff_arena_init_heap_global(&dest, 4096);

            ff_string_view copied{ "", 0 };
            {
                ff_arena scratch;
                ff_arena_init_heap_global(&scratch, 4096);

                ff_string_builder sb;
                ff_string_builder_init(&sb, &scratch);
                ff_string_builder_append(&sb, FF_SVL("persist me"));

                copied = ff_string_builder_copy_to(&sb, &dest);

                ff_arena_destroy(&scratch); // the builder's arena is gone, but 'copied' lives in 'dest'
            }

            Assert::IsTrue(::view_equals(copied, "persist me"));
            Assert::AreEqual((size_t)10, copied.count);
            Assert::AreEqual("persist me", copied.data); // still a valid C-string

            ff_arena_destroy(&dest);
        }

        TEST_METHOD(copy_empty_returns_empty_string)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);

            ff_string_view copied = ff_string_builder_copy(&sb);
            Assert::IsNotNull(copied.data);
            Assert::AreEqual((size_t)0, copied.count);
            Assert::AreEqual('\0', copied.data[0]); // null-terminated
            Assert::AreEqual("", copied.data);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(copy_result_usable_as_view_and_cstring)
        {
            ff_arena arena;
            ff_arena_init_heap_global(&arena, 4096);

            ff_string_builder sb;
            ff_string_builder_init(&sb, &arena);
            ff_string_builder_append(&sb, FF_SVL("hello"));

            ff_string_view copied = ff_string_builder_copy(&sb);
            Assert::IsNotNull(copied.data);

            // The view carries the length...
            Assert::IsTrue(::view_equals(copied, "hello"));
            // ...and the terminator past the count makes it a valid C-string.
            Assert::AreEqual('\0', copied.data[copied.count]);
            Assert::AreEqual("hello", copied.data);

            ff_arena_destroy(&arena);
        }
    };
}
