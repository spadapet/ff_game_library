#include "pch.h"

// Tests for ff_compress / ff_uncompress / ff_decode_base64.
// Coverage:
//   * Round trips through memory streams, including empty, tiny, and multi-chunk payloads.
//   * Incompressible (random) data still round trips even though it grows.
//   * Uncompressing truncated or garbage input fails instead of returning partial data.
//   * Base64 decodes with zero, one, and two padding characters.

namespace ff::test::base
{
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
