#include "pch.h"

// Compare a string_builder's view to a null-terminated expected string.
static bool view_equals(ff::string_view view, const char* expected)
{
    size_t expected_len = ::strlen(expected);
    if (view.size != expected_len)
    {
        return false;
    }

    return ::memcmp(view.data, expected, expected_len) == 0;
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
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            Assert::AreEqual((void*)&arena, (void*)sb.arena);
            Assert::IsNotNull(sb.data);
            Assert::AreEqual((size_t)0, sb.size);
            Assert::IsTrue(sb.capacity > 0);
            Assert::AreEqual("", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(init_custom_capacity)
        {
            ff::arena arena;
            arena.init_heap(8192);

            ff::string_builder sb;
            sb.init(&arena, 4096);

            Assert::IsTrue(sb.capacity >= 4096 + 1);
            Assert::AreEqual((size_t)0, sb.size);

            arena.destroy();
        }

        TEST_METHOD(init_with_string_view_seed)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena, ff::string_view{ "hello", 5 });

            Assert::AreEqual((size_t)5, sb.size);
            Assert::AreEqual("hello", sb.c_str());

            // Can keep appending after a seeded init.
            sb.append(FF_SVL(" world"));
            Assert::AreEqual("hello world", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(init_with_empty_string_view)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena, ff::string_view{ nullptr, 0 });

            Assert::AreEqual((size_t)0, sb.size);
            Assert::AreEqual("", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(init_seed_larger_than_default_capacity)
        {
            ff::arena arena;
            arena.init_heap(8192);

            // Seed bigger than the default initial capacity must still fit in one buffer.
            char big[2000];
            for (int i = 0; i < 2000; ++i)
            {
                big[i] = (char)('a' + (i % 26));
            }

            ff::string_builder sb;
            sb.init(&arena, ff::string_view{ big, sizeof(big) });

            Assert::AreEqual((size_t)2000, sb.size);
            Assert::IsTrue(::memcmp(sb.data, big, sizeof(big)) == 0);

            arena.destroy();
        }

        // ====================================================================
        // Append
        // ====================================================================
        TEST_METHOD(append_char)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            sb.append('a')->append('b')->append('c');

            Assert::AreEqual((size_t)3, sb.size);
            Assert::AreEqual("abc", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(append_cstr)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            sb.append(FF_SVL("hello"))->append(FF_SVL(", "))->append(FF_SVL("world"));

            Assert::AreEqual("hello, world", sb.c_str());
            Assert::AreEqual((size_t)12, sb.size);

            arena.destroy();
        }

        TEST_METHOD(append_string_view)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            ff::string_view first{ "foo", 3 };
            ff::string_view second{ "bar", 3 };
            sb.append(first)->append(second);

            Assert::AreEqual("foobar", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(append_partial_string_view)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            const char* text = "abcdefgh";
            sb.append(ff::string_view{ text, 3 }); // "abc"
            sb.append(ff::string_view{ text + 5, 3 }); // "fgh"

            Assert::AreEqual("abcfgh", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(append_null_cstr_is_noop)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            sb.append(FF_SVL("keep"));
            sb.append(ff::sz_view((const char*)nullptr));

            Assert::AreEqual("keep", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(append_empty_is_noop)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            sb.append(FF_SVL("keep"));
            sb.append(ff::string_view{ "", 0 });

            Assert::AreEqual((size_t)4, sb.size);
            Assert::AreEqual("keep", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // view() / c_str()
        // ====================================================================
        TEST_METHOD(view_returns_start_size)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("abcdef"));

            ff::string_view view = sb.view();
            Assert::AreEqual((size_t)6, view.size);
            Assert::IsTrue(::view_equals(view, "abcdef"));
            Assert::AreEqual((void*)sb.data, (void*)view.data);

            arena.destroy();
        }

        TEST_METHOD(c_str_terminates_without_changing_size)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("data"));

            const char* result = sb.c_str();
            Assert::AreEqual('\0', result[4]);
            Assert::AreEqual((size_t)4, sb.size); // size excludes the terminator

            // Appending after c_str() overwrites the terminator slot correctly.
            sb.append('!');
            Assert::AreEqual("data!", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // Insert
        // ====================================================================
        TEST_METHOD(insert_at_front)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("world"));

            sb.insert(0, FF_SVL("hello "));

            Assert::AreEqual("hello world", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(insert_in_middle)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("helloworld"));

            sb.insert(5, FF_SVL(", "));

            Assert::AreEqual("hello, world", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(insert_at_end)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("hello"));

            sb.insert(sb.size, FF_SVL("!!!"));

            Assert::AreEqual("hello!!!", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(insert_char)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("ac"));

            sb.insert(1, 'b');

            Assert::AreEqual("abc", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(insert_string_view)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("AC"));

            ff::string_view middle{ "B", 1 };
            sb.insert(1, middle);

            Assert::AreEqual("ABC", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // Remove
        // ====================================================================
        TEST_METHOD(remove_from_middle)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("hello, world"));

            sb.remove(5, 2); // remove ", "

            Assert::AreEqual("helloworld", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(remove_from_front)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("XXabc"));

            sb.remove(0, 2);

            Assert::AreEqual("abc", sb.c_str());

            arena.destroy();
        }

        TEST_METHOD(remove_count_clamped_to_end)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("abcdef"));

            sb.remove(3, 100); // count past end => trims to end

            Assert::AreEqual("abc", sb.c_str());
            Assert::AreEqual((size_t)3, sb.size);

            arena.destroy();
        }

        TEST_METHOD(remove_zero_is_noop)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("abc"));

            sb.remove(1, 0);

            Assert::AreEqual("abc", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // Reset
        // ====================================================================
        TEST_METHOD(reset_clears_but_keeps_buffer)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);
            sb.append(FF_SVL("some content"));

            char* data_before = sb.data;
            size_t capacity_before = sb.capacity;

            sb.reset();

            Assert::AreEqual((size_t)0, sb.size);
            Assert::AreEqual("", sb.c_str());
            Assert::AreEqual((void*)data_before, (void*)sb.data); // same buffer reused
            Assert::AreEqual(capacity_before, sb.capacity);

            sb.append(FF_SVL("reused"));
            Assert::AreEqual("reused", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // reserve / growth
        // ====================================================================
        TEST_METHOD(reserve_grows_capacity)
        {
            ff::arena arena;
            arena.init_heap(64);

            ff::string_builder sb;
            sb.init(&arena, 16);

            sb.reserve(10000);
            Assert::IsTrue(sb.capacity >= 10000 + 1);
            Assert::AreEqual((size_t)0, sb.size);

            arena.destroy();
        }

        TEST_METHOD(growth_doubles_and_preserves_content)
        {
            ff::arena arena;
            arena.init_heap(64);

            ff::string_builder sb;
            sb.init(&arena, 16);

            size_t initial_capacity = sb.capacity;

            // Append enough to force several doublings.
            for (int i = 0; i < 1000; ++i)
            {
                sb.append('x');
            }

            Assert::AreEqual((size_t)1000, sb.size);
            Assert::IsTrue(sb.capacity > initial_capacity);

            // Every char should still be 'x' and the string null-terminated.
            const char* result = sb.c_str();
            for (int i = 0; i < 1000; ++i)
            {
                Assert::AreEqual('x', result[i]);
            }
            Assert::AreEqual('\0', result[1000]);

            arena.destroy();
        }

        TEST_METHOD(growth_preserves_content_arena)
        {
            ff::arena arena;
            arena.init_heap(64);

            ff::string_builder sb;
            sb.init(&arena, 8); // small so growth via arena::realloc happens

            for (int i = 0; i < 500; ++i)
            {
                sb.append((char)('a' + (i % 26)));
            }

            Assert::AreEqual((size_t)500, sb.size);

            const char* result = sb.c_str();
            for (int i = 0; i < 500; ++i)
            {
                Assert::AreEqual((char)('a' + (i % 26)), result[i]);
            }

            arena.destroy();
        }

        TEST_METHOD(arena_relocation_on_interleaved_alloc)
        {
            // When another arena allocation happens between appends, the builder is no longer the
            // arena's last allocation, so the next grow relocates the buffer. Content must survive.
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena, 8);
            sb.append(FF_SVL("12345678")); // fill to initial capacity to force a grow next time

            // Interleave an unrelated arena allocation.
            void* other = arena.alloc(64, 8);
            Assert::IsNotNull(other);

            // This append forces a grow; since 'other' is now last, the builder relocates.
            sb.append(FF_SVL("ABCDEFGH"));

            Assert::AreEqual("12345678ABCDEFGH", sb.c_str());

            arena.destroy();
        }

        // ====================================================================
        // Combined / round-trip
        // ====================================================================
        TEST_METHOD(append_insert_remove_round_trip)
        {
            ff::arena arena;
            arena.init_heap(4096);

            ff::string_builder sb;
            sb.init(&arena);

            sb.append(FF_SVL("The quick fox"));
            sb.insert(10, FF_SVL("brown ")); // "The quick brown fox"
            Assert::AreEqual("The quick brown fox", sb.c_str());

            sb.remove(3, 6); // remove " quick" => "The brown fox"
            Assert::AreEqual("The brown fox", sb.c_str());

            sb.append('!');
            Assert::AreEqual("The brown fox!", sb.c_str());

            arena.destroy();
        }
    };
}
