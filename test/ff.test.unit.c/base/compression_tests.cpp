#include "pch.h"

// Tests for ff_compress / ff_uncompress / ff_decode_base64.
// Coverage:
//   * Round trips through memory streams, including empty, tiny, and multi-chunk payloads.
//   * Incompressible (random) data still round trips even though it grows.
//   * Uncompressing truncated or garbage input fails instead of returning partial data.
//   * Base64 decodes with zero, one, and two padding characters.

namespace ff::test::base
{
    struct scoped_compression_file
    {
        wchar_t path[MAX_PATH]{};
        ff_stream reader{};

        scoped_compression_file()
        {
            wchar_t directory[MAX_PATH];
            const DWORD count = GetTempPathW(_countof(directory), directory);
            Assert::IsTrue(count > 0 && count < _countof(directory));
            Assert::IsTrue(GetTempFileNameW(directory, L"ffc", 0, this->path) != 0);
        }

        ~scoped_compression_file()
        {
            ff_stream_destroy(&this->reader);
            DeleteFileW(this->path);
        }

        void init_read(ff_span data)
        {
            ff_arena_declare_stack(path_arena, 2048);
            const ff_string_view filename = ff_wide_to_utf8(ff_wz_view(this->path), &path_arena, true);
            ff_stream writer{};
            Assert::IsTrue(ff_stream_init_write_file(&writer, filename));
            const bool written = ff_stream_write(&writer, data);
            ff_stream_destroy(&writer);
            Assert::IsTrue(written);
            Assert::IsTrue(ff_stream_init_read_file(&this->reader, filename));
            ff_arena_destroy(&path_arena);
        }
    };

    static ff_span span_of(const void* data, size_t size)
    {
        ff_span span;
        span.data = data;
        span.size = size;
        return span;
    }

    // Compresses then uncompresses, asserting the result matches the original byte for byte.
    static void assert_round_trips(const void* data, size_t size)
    {
        ff_arena arena;
        ff_arena_init_heap_local(&arena, 0);

        ff_stream reader;
        ff_stream_init_read_memory(&reader, span_of(data, size));

        ff_stream compressed;
        ff_stream_init_write_memory(&compressed, &arena, 0);
        Assert::IsTrue(ff_compress(&reader, size, &compressed));

        ff_stream compressed_reader;
        ff_stream_init_read_memory(&compressed_reader, ff_stream_written(&compressed));

        ff_stream result;
        ff_stream_init_write_memory(&result, &arena, 0);
        Assert::IsTrue(ff_uncompress(&compressed_reader, ff_stream_size(&compressed), &result));

        const ff_span written = ff_stream_written(&result);
        Assert::AreEqual(size, written.size);

        if (size)
        {
            Assert::AreEqual(0, ::memcmp(written.data, data, size));
        }

        ff_stream_destroy(&reader);
        ff_stream_destroy(&compressed);
        ff_stream_destroy(&compressed_reader);
        ff_stream_destroy(&result);
        ff_arena_destroy(&arena);
    }

    TEST_CLASS(compression_tests)
    {
    public:
        TEST_METHOD(round_trip_empty)
        {
            assert_round_trips(nullptr, 0);
        }

        TEST_METHOD(round_trip_one_byte)
        {
            const uint8_t original = 42;
            assert_round_trips(&original, sizeof(original));
        }

        TEST_METHOD(round_trip_stack_scratch_boundaries)
        {
            uint8_t original[8192];
            uint32_t seed = 12345;
            for (size_t i = 0; i < sizeof(original); i++)
            {
                seed = seed * 1664525u + 1013904223u;
                original[i] = (uint8_t)(seed >> 16);
            }
            const size_t sizes[] = { 128, 1024, 2048, 4095, 4096, 4097, sizeof(original) };
            for (size_t size : sizes)
            {
                assert_round_trips(original, size);
            }
        }

        TEST_METHOD(round_trip_compressible_data)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const size_t size = 10000;
            uint8_t* original = ff_arena_alloc_type(&arena, uint8_t, size);
            ::memset(original, 'A', size);

            assert_round_trips(original, size);
            ff_arena_destroy(&arena);
        }

        // Larger than the 256KB chunk size, so the chunk loop runs more than once.
        TEST_METHOD(round_trip_multiple_chunks)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const size_t size = 1024 * 1024;
            uint8_t* original = ff_arena_alloc_type(&arena, uint8_t, size);

            for (size_t i = 0; i < size; i++)
            {
                original[i] = (uint8_t)(i * 7);
            }

            assert_round_trips(original, size);
            ff_arena_destroy(&arena);
        }

        // Random data can't be compressed, so zlib's output for one input chunk overflows the
        // output buffer and the drain loop has to run more than once.
        TEST_METHOD(round_trip_incompressible_data)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const size_t size = 300 * 1024;
            uint8_t* original = ff_arena_alloc_type(&arena, uint8_t, size);
            uint32_t seed = 12345;

            for (size_t i = 0; i < size; i++)
            {
                seed = seed * 1664525 + 1013904223;
                original[i] = (uint8_t)(seed >> 16);
            }

            assert_round_trips(original, size);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(file_round_trips_stack_spill_and_multiple_chunks)
        {
            ff_arena_declare_stack(arena, 4096);
            const size_t sizes[] = { 128, 8192, 600 * 1024 };
            uint8_t* original = ff_arena_alloc_type(&arena, uint8_t, sizes[2]);
            uint32_t seed = 12345;
            for (size_t i = 0; i < sizes[2]; i++)
            {
                seed = seed * 1664525u + 1013904223u;
                original[i] = (uint8_t)(seed >> 16);
            }
            for (size_t size : sizes)
            {
                scoped_compression_file input;
                input.init_read(span_of(original, size));
                ff_stream compressed{};
                ff_stream_init_write_memory(&compressed, &arena, 0);
                Assert::IsTrue(ff_compress(&input.reader, size, &compressed));
                scoped_compression_file saved;
                saved.init_read(ff_stream_written(&compressed));
                ff_stream result{};
                ff_stream_init_write_memory(&result, &arena, 0);
                Assert::IsTrue(ff_uncompress(&saved.reader, ff_stream_size(&saved.reader), &result));
                const ff_span written = ff_stream_written(&result);
                Assert::AreEqual(size, written.size);
                Assert::AreEqual(0, memcmp(original, written.data, size));
                ff_stream_destroy(&result);
                ff_stream_destroy(&compressed);
            }
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(uncompress_garbage_fails)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const size_t size = 64;
            uint8_t* garbage = ff_arena_alloc_type(&arena, uint8_t, size);
            ::memset(garbage, 0xCD, size);

            ff_stream reader;
            ff_stream_init_read_memory(&reader, span_of(garbage, size));

            ff_stream result;
            ff_stream_init_write_memory(&result, &arena, 0);

            Assert::IsFalse(ff_uncompress(&reader, size, &result));

            ff_stream_destroy(&reader);
            ff_stream_destroy(&result);
            ff_arena_destroy(&arena);
        }

        // A truncated stream never reaches Z_STREAM_END, so it must fail rather than quietly
        // returning the bytes that did decode.
        TEST_METHOD(uncompress_truncated_fails)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const size_t size = 10000;
            uint8_t* original = ff_arena_alloc_type(&arena, uint8_t, size);
            ::memset(original, 'B', size);

            ff_stream reader;
            ff_stream_init_read_memory(&reader, span_of(original, size));

            ff_stream compressed;
            ff_stream_init_write_memory(&compressed, &arena, 0);
            Assert::IsTrue(ff_compress(&reader, size, &compressed));

            const size_t truncated_size = ff_stream_size(&compressed) / 2;
            Assert::IsTrue(truncated_size > 0);

            ff_stream truncated_reader;
            ff_stream_init_read_memory(&truncated_reader, ff_stream_written(&compressed));

            ff_stream result;
            ff_stream_init_write_memory(&result, &arena, 0);

            Assert::IsFalse(ff_uncompress(&truncated_reader, truncated_size, &result));

            ff_stream_destroy(&reader);
            ff_stream_destroy(&compressed);
            ff_stream_destroy(&truncated_reader);
            ff_stream_destroy(&result);
            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_no_padding)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_span decoded = ff_decode_base64(FF_SVL("TWFu"), &arena);
            Assert::AreEqual((size_t)3, decoded.size);
            Assert::AreEqual(0, ::memcmp(decoded.data, "Man", 3));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_one_pad)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_span decoded = ff_decode_base64(FF_SVL("TWFuYQ=="), &arena);
            Assert::AreEqual((size_t)4, decoded.size);
            Assert::AreEqual(0, ::memcmp(decoded.data, "Mana", 4));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_two_pad)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_span decoded = ff_decode_base64(FF_SVL("TWFuYWc="), &arena);
            Assert::AreEqual((size_t)5, decoded.size);
            Assert::AreEqual(0, ::memcmp(decoded.data, "Manag", 5));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_empty)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_span decoded = ff_decode_base64(ff_string_view_empty(), &arena);
            Assert::AreEqual((size_t)0, decoded.size);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_rejects_invalid_input)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            const ff_string_view invalid[] =
            {
                FF_SVL("A"),
                FF_SVL("AAA"),
                FF_SVL("AAAAA"),
                FF_SVL("!AAA"),
                FF_SVL("A!AA"),
                FF_SVL("AA!A"),
                FF_SVL("AAA!"),
                FF_SVL("AAAAAA!A"),
                FF_SVL("AAA "),
                FF_SVL("AA\nA"),
                FF_SVL("AA\0A"),
                FF_SVL("AA\xFF" "A"),
                FF_SVL("=AAA"),
                FF_SVL("A=AA"),
                FF_SVL("AA=A"),
                FF_SVL("A==="),
                FF_SVL("===="),
                FF_SVL("AA==AAAA"),
                FF_SVL("AAA=AAAA"),
                FF_SVL("AA==AA=="),
                FF_SVL("AAA=AAA="),
                FF_SVL("AAAAA==="),
            };

            for (ff_string_view text : invalid)
            {
                const ff_span decoded = ff_decode_base64(text, &arena);
                Assert::IsNull(decoded.data);
                Assert::AreEqual((size_t)0, decoded.size);
            }

            const ff_span decoded = ff_decode_base64(FF_SVL("TWFu"), &arena);
            Assert::AreEqual((size_t)3, decoded.size);
            Assert::AreEqual(0, ::memcmp(decoded.data, "Man", decoded.size));

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(decode_base64_padded_output_stays_in_bounds)
        {
            const ff_string_view encoded[] =
            {
                FF_SVL("Zg=="),
                FF_SVL("Zm8="),
                FF_SVL("Zm9v"),
                FF_SVL("Zm9vYg=="),
                FF_SVL("Zm9vYmE="),
                FF_SVL("Zm9vYmFy"),
            };
            const char* expected[] = { "f", "fo", "foo", "foob", "fooba", "foobar" };

            for (size_t i = 0; i < std::size(encoded); i++)
            {
                uint8_t buffer[256];
                ::memset(buffer, 0xCD, sizeof(buffer));
                ff_arena arena;
                ff_arena_init_external(&arena, buffer, sizeof(buffer), 0);
                ff_arena_marker before = ff_arena_mark(&arena);

                const ff_span decoded = ff_decode_base64(encoded[i], &arena);
                Assert::AreEqual(::strlen(expected[i]), decoded.size);
                Assert::AreEqual(0, ::memcmp(decoded.data, expected[i], decoded.size));
                Assert::IsTrue(decoded.data == before);
                Assert::IsTrue(ff_arena_mark(&arena) == before + decoded.size);
                Assert::AreEqual((uint8_t)0xCD, before[decoded.size]);

                ff_arena_destroy(&arena);
            }
        }

        TEST_METHOD(decode_base64_non_alphanumeric_chars)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            // '/' and '+' are the two characters outside the alphanumeric ranges.
            const ff_span decoded = ff_decode_base64(FF_SVL("/+8="), &arena);
            Assert::AreEqual((size_t)2, decoded.size);
            Assert::AreEqual((int)0xFF, (int)((const uint8_t*)decoded.data)[0]);
            Assert::AreEqual((int)0xEF, (int)((const uint8_t*)decoded.data)[1]);

            ff_arena_destroy(&arena);
        }
    };
}
