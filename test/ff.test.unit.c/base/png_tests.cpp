#include "pch.h"
#include <libpng/png.h>

namespace ff::test::base
{
    // Growable sink for the encoder below. The tests build real PNG files with libpng rather than
    // checking in binary fixtures or hand-assembling chunks, so the decoder is always exercised
    // against genuinely well-formed input.
    struct png_sink
    {
        uint8_t* data;
        size_t size;
        size_t capacity;
    };

    static void sink_write(png_structp png, png_bytep bytes, size_t count)
    {
        png_sink* sink = (png_sink*)png_get_io_ptr(png);

        if (sink->size + count > sink->capacity)
        {
            size_t capacity = sink->capacity ? sink->capacity * 2 : 4096;

            while (capacity < sink->size + count)
            {
                capacity *= 2;
            }

            uint8_t* data = (uint8_t*)realloc(sink->data, capacity);
            Assert::IsNotNull(data);

            sink->data = data;
            sink->capacity = capacity;
        }

        memcpy(sink->data + sink->size, bytes, count);
        sink->size += count;
    }

    static void sink_flush(png_structp)
    {
    }

    TEST_CLASS(png_tests)
    {
    public:
        // Encodes rows into a PNG in memory. 'color_type' and 'bit_depth' are passed straight to
        // libpng, so a test can produce exactly the input it wants to decode.
        static png_sink encode(uint32_t width, uint32_t height, int color_type, int bit_depth,
            const uint8_t* pixels, size_t row_pitch,
            const png_color* palette = nullptr, int palette_size = 0,
            const uint8_t* trans = nullptr, int trans_size = 0,
            int interlace = PNG_INTERLACE_NONE)
        {
            png_sink sink{};

            png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            Assert::IsNotNull(png);

            png_infop info = png_create_info_struct(png);
            Assert::IsNotNull(info);

            png_set_write_fn(png, &sink, sink_write, sink_flush);
            png_set_IHDR(png, info, width, height, bit_depth, color_type, interlace,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

            if (palette && palette_size)
            {
                png_set_PLTE(png, info, palette, palette_size);

                if (trans && trans_size)
                {
                    png_set_tRNS(png, info, (png_bytep)trans, trans_size, nullptr);
                }
            }

            png_write_info(png, info);

            // Interlaced output is written in several passes over the same rows.
            const int passes = png_set_interlace_handling(png);

            for (int pass = 0; pass < passes; pass++)
            {
                for (uint32_t y = 0; y < height; y++)
                {
                    png_write_row(png, (png_bytep)(pixels + (size_t)y * row_pitch));
                }
            }

            png_write_end(png, info);
            png_destroy_write_struct(&png, &info);

            return sink;
        }

        static ff_span span_of(const png_sink& sink)
        {
            ff_span span;
            span.data = sink.data;
            span.size = sink.size;
            return span;
        }

        TEST_METHOD(decodes_rgba_and_preserves_every_channel)
        {
            const uint32_t width = 4;
            const uint32_t height = 3;
            uint8_t source[width * height * 4];

            for (size_t i = 0; i < width * height; i++)
            {
                source[i * 4 + 0] = (uint8_t)(i * 7);
                source[i * 4 + 1] = (uint8_t)(i * 11);
                source[i * 4 + 2] = (uint8_t)(i * 13);
                source[i * 4 + 3] = (uint8_t)(i * 17);
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, width * 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            const bool decoded = ff_png_decode(span_of(sink), &arena, false, &image);

            Assert::IsTrue(decoded);
            Assert::AreEqual((uint32_t)width, image.width);
            Assert::AreEqual((uint32_t)height, image.height);
            Assert::AreEqual(0, memcmp(image.pixels, source, sizeof(source)));

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Catches a swapped red/blue channel, which is the classic PNG-to-DXGI mistake and is
        // invisible to a test that only checks dimensions.
        TEST_METHOD(channel_order_is_rgba_not_bgra)
        {
            const uint8_t source[4] = { 0xFF, 0x00, 0x00, 0xFF };
            png_sink sink = encode(1, 1, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::AreEqual((int)0xFF, (int)image.pixels[0]);
            Assert::AreEqual((int)0x00, (int)image.pixels[1]);
            Assert::AreEqual((int)0x00, (int)image.pixels[2]);
            Assert::AreEqual((int)0xFF, (int)image.pixels[3]);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(rgb_without_alpha_becomes_opaque_rgba)
        {
            const uint32_t width = 3;
            const uint32_t height = 2;
            uint8_t source[width * height * 3];

            for (size_t i = 0; i < sizeof(source); i++)
            {
                source[i] = (uint8_t)(i * 3);
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB, 8, source, width * 3);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            for (size_t i = 0; i < width * height; i++)
            {
                Assert::AreEqual((int)source[i * 3 + 0], (int)image.pixels[i * 4 + 0]);
                Assert::AreEqual((int)source[i * 3 + 1], (int)image.pixels[i * 4 + 1]);
                Assert::AreEqual((int)source[i * 3 + 2], (int)image.pixels[i * 4 + 2]);
                Assert::AreEqual((int)0xFF, (int)image.pixels[i * 4 + 3]);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(grayscale_expands_across_all_three_color_channels)
        {
            const uint8_t source[2] = { 0x40, 0xC0 };
            png_sink sink = encode(2, 1, PNG_COLOR_TYPE_GRAY, 8, source, 2);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            for (size_t i = 0; i < 2; i++)
            {
                Assert::AreEqual((int)source[i], (int)image.pixels[i * 4 + 0]);
                Assert::AreEqual((int)source[i], (int)image.pixels[i * 4 + 1]);
                Assert::AreEqual((int)source[i], (int)image.pixels[i * 4 + 2]);
                Assert::AreEqual((int)0xFF, (int)image.pixels[i * 4 + 3]);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(sixteen_bit_channels_are_stripped_to_eight)
        {
            // Big endian, as the PNG format requires.
            const uint8_t source[6] = { 0xAB, 0xCD, 0x12, 0x34, 0x56, 0x78 };
            png_sink sink = encode(1, 1, PNG_COLOR_TYPE_RGB, 16, source, 6);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::AreEqual((int)0xAB, (int)image.pixels[0]);
            Assert::AreEqual((int)0x12, (int)image.pixels[1]);
            Assert::AreEqual((int)0x56, (int)image.pixels[2]);
            Assert::AreEqual((int)0xFF, (int)image.pixels[3]);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        static void make_palette(png_color* palette, int count)
        {
            for (int i = 0; i < count; i++)
            {
                palette[i].red = (uint8_t)(i * 10);
                palette[i].green = (uint8_t)(i * 20);
                palette[i].blue = (uint8_t)(i * 30);
            }
        }

        TEST_METHOD(indexed_png_expands_to_rgba_when_the_palette_is_not_kept)
        {
            png_color palette[4];
            make_palette(palette, 4);

            const uint8_t indexes[4] = { 0, 1, 2, 3 };
            png_sink sink = encode(4, 1, PNG_COLOR_TYPE_PALETTE, 8, indexes, 4, palette, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::IsFalse(image.has_palette);
            Assert::IsNull(image.indexes);

            for (int i = 0; i < 4; i++)
            {
                Assert::AreEqual((int)palette[i].red, (int)image.pixels[i * 4 + 0]);
                Assert::AreEqual((int)palette[i].green, (int)image.pixels[i * 4 + 1]);
                Assert::AreEqual((int)palette[i].blue, (int)image.pixels[i * 4 + 2]);
                Assert::AreEqual((int)0xFF, (int)image.pixels[i * 4 + 3]);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // The palette renderer needs the raw indexes, not flattened colors. This is exactly what
        // WIC could not provide and why libpng was chosen.
        TEST_METHOD(indexed_png_keeps_indexes_and_palette_when_asked)
        {
            png_color palette[4];
            make_palette(palette, 4);

            const uint8_t indexes[4] = { 3, 1, 0, 2 };
            png_sink sink = encode(4, 1, PNG_COLOR_TYPE_PALETTE, 8, indexes, 4, palette, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, true, &image));

            Assert::IsTrue(image.has_palette);
            Assert::IsNotNull(image.indexes);
            Assert::AreEqual((uint32_t)4, image.palette_size);
            Assert::AreEqual(0, memcmp(image.indexes, indexes, sizeof(indexes)));

            for (int i = 0; i < 4; i++)
            {
                Assert::AreEqual((int)palette[i].red, (int)image.palette[i * 4 + 0]);
                Assert::AreEqual((int)palette[i].green, (int)image.palette[i * 4 + 1]);
                Assert::AreEqual((int)palette[i].blue, (int)image.palette[i * 4 + 2]);
                Assert::AreEqual((int)0xFF, (int)image.palette[i * 4 + 3]);
            }
        
            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Both outputs must agree, so a caller can use either without knowing how it was decoded.
        TEST_METHOD(kept_indexes_still_produce_matching_rgba_pixels)
        {
            png_color palette[4];
            make_palette(palette, 4);

            const uint8_t indexes[4] = { 3, 1, 0, 2 };
            png_sink sink = encode(4, 1, PNG_COLOR_TYPE_PALETTE, 8, indexes, 4, palette, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, true, &image));

            for (int i = 0; i < 4; i++)
            {
                const int index = indexes[i];
                Assert::AreEqual((int)palette[index].red, (int)image.pixels[i * 4 + 0]);
                Assert::AreEqual((int)palette[index].green, (int)image.pixels[i * 4 + 1]);
                Assert::AreEqual((int)palette[index].blue, (int)image.pixels[i * 4 + 2]);
                Assert::AreEqual((int)0xFF, (int)image.pixels[i * 4 + 3]);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(palette_transparency_is_carried_into_the_palette_alpha)
        {
            png_color palette[4];
            make_palette(palette, 4);

            const uint8_t trans[2] = { 0x00, 0x80 };
            const uint8_t indexes[4] = { 0, 1, 2, 3 };
            png_sink sink = encode(4, 1, PNG_COLOR_TYPE_PALETTE, 8, indexes, 4, palette, 4, trans, 2);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, true, &image));

            Assert::AreEqual((int)0x00, (int)image.palette[0 * 4 + 3]);
            Assert::AreEqual((int)0x80, (int)image.palette[1 * 4 + 3]);

            // Entries past the tRNS chunk default to opaque.
            Assert::AreEqual((int)0xFF, (int)image.palette[2 * 4 + 3]);
            Assert::AreEqual((int)0xFF, (int)image.palette[3 * 4 + 3]);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Sub-byte indexes are packed several to a byte in the file and must be unpacked.
        TEST_METHOD(four_bit_indexed_png_unpacks_to_one_index_per_byte)
        {
            png_color palette[4];
            make_palette(palette, 4);

            // Four pixels at 4 bits each: indexes 0, 1, 2, 3.
            const uint8_t packed[2] = { 0x01, 0x23 };
            png_sink sink = encode(4, 1, PNG_COLOR_TYPE_PALETTE, 4, packed, 2, palette, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, true, &image));

            Assert::IsTrue(image.has_palette);

            for (int i = 0; i < 4; i++)
            {
                Assert::AreEqual(i, (int)image.indexes[i]);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(interlaced_png_is_rejected)
        {
            const uint8_t source[4 * 4 * 4] = { 0 };
            png_sink sink = encode(4, 4, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, 4 * 4,
                nullptr, 0, nullptr, 0, PNG_INTERLACE_ADAM7);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            const bool decoded = ff_png_decode(span_of(sink), &arena, false, &image);

            Assert::IsFalse(decoded);
            Assert::AreEqual((uint32_t)0, image.width);
            Assert::IsNull(image.pixels);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(non_png_data_is_rejected)
        {
            const char text[] = "this is definitely not a png file at all";

            ff_span span;
            span.data = text;
            span.size = sizeof(text);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsFalse(ff_png_decode(span, &arena, false, &image));
            Assert::IsNull(image.pixels);

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(empty_and_tiny_input_is_rejected)
        {
            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};

            ff_span empty;
            empty.data = nullptr;
            empty.size = 0;
            Assert::IsFalse(ff_png_decode(empty, &arena, false, &image));

            const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
            ff_span tiny;
            tiny.data = signature;
            tiny.size = sizeof(signature);
            Assert::IsFalse(ff_png_decode(tiny, &arena, false, &image));

            ff_arena_destroy(&arena);
        }

        // A file cut short mid-stream must fail through libpng's longjmp path rather than crash
        // or return a half-filled image.
        TEST_METHOD(truncated_png_fails_cleanly)
        {
            const uint32_t width = 16;
            const uint32_t height = 16;
            uint8_t source[width * height * 4];

            for (size_t i = 0; i < sizeof(source); i++)
            {
                source[i] = (uint8_t)i;
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, width * 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_span truncated;
            truncated.data = sink.data;
            truncated.size = sink.size / 2;

            ff_png_image image{};
            Assert::IsFalse(ff_png_decode(truncated, &arena, false, &image));
            Assert::IsNull(image.pixels);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Corruption in the compressed data trips a CRC failure deep inside libpng, which is the
        // error path most likely to leak or crash if the longjmp cleanup is wrong.
        TEST_METHOD(corrupt_png_data_fails_cleanly)
        {
            const uint32_t width = 16;
            const uint32_t height = 16;
            uint8_t source[width * height * 4];

            for (size_t i = 0; i < sizeof(source); i++)
            {
                source[i] = (uint8_t)(i * 5);
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, width * 4);

            // Well past the signature and header, so the damage lands in the image data.
            for (size_t i = sink.size / 2; i < sink.size / 2 + 16 && i < sink.size; i++)
            {
                sink.data[i] ^= 0xFF;
            }

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            const bool decoded = ff_png_decode(span_of(sink), &arena, false, &image);

            // Either outcome is legal, but the struct must never be half-filled.
            if (decoded)
            {
                Assert::IsNotNull(image.pixels);
                Assert::AreEqual(width, image.width);
            }
            else
            {
                Assert::IsNull(image.pixels);
                Assert::AreEqual((uint32_t)0, image.width);
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Repeated failures must not accumulate anything. If the longjmp path leaked a png struct
        // this would grow without bound.
        TEST_METHOD(repeated_failed_decodes_do_not_leak)
        {
            const char text[] = "not a png";

            ff_span span;
            span.data = text;
            span.size = sizeof(text);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            for (size_t i = 0; i < 500; i++)
            {
                ff_png_image image{};
                Assert::IsFalse(ff_png_decode(span, &arena, false, &image));
            }

            ff_arena_destroy(&arena);
        }

        TEST_METHOD(a_tall_image_spills_the_row_array_past_the_stack_arena)
        {
            // Taller than PNG_STACK_ROWS, so the row pointer array must come from the heap.
            const uint32_t width = 1;
            const uint32_t height = 3000;

            uint8_t* source = (uint8_t*)malloc((size_t)width * height * 4);
            Assert::IsNotNull(source);

            for (size_t i = 0; i < (size_t)width * height * 4; i++)
            {
                source[i] = (uint8_t)i;
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, width * 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::AreEqual(height, image.height);
            Assert::AreEqual(0, memcmp(image.pixels, source, (size_t)width * height * 4));

            ff_arena_destroy(&arena);
            free(sink.data);
            free(source);
        }

        TEST_METHOD(a_single_pixel_image_decodes)
        {
            const uint8_t source[4] = { 0x11, 0x22, 0x33, 0x44 };
            png_sink sink = encode(1, 1, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::AreEqual((uint32_t)1, image.width);
            Assert::AreEqual((uint32_t)1, image.height);
            Assert::AreEqual(0, memcmp(image.pixels, source, 4));

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        // Rows must be tightly packed, since the caller uploads them straight to a texture.
        TEST_METHOD(rows_are_tightly_packed_with_no_padding)
        {
            const uint32_t width = 3;
            const uint32_t height = 3;
            uint8_t source[width * height * 4];

            for (size_t i = 0; i < sizeof(source); i++)
            {
                source[i] = (uint8_t)(i + 1);
            }

            png_sink sink = encode(width, height, PNG_COLOR_TYPE_RGB_ALPHA, 8, source, width * 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            for (uint32_t y = 0; y < height; y++)
            {
                Assert::AreEqual(0, memcmp(
                    image.pixels + (size_t)y * width * 4,
                    source + (size_t)y * width * 4,
                    width * 4));
            }

            ff_arena_destroy(&arena);
            free(sink.data);
        }

        TEST_METHOD(gray_with_alpha_expands_and_keeps_its_alpha)
        {
            const uint8_t source[4] = { 0x80, 0x40, 0x20, 0xC0 };
            png_sink sink = encode(2, 1, PNG_COLOR_TYPE_GRAY_ALPHA, 8, source, 4);

            ff_arena arena;
            ff_arena_init_heap_local(&arena, 0);

            ff_png_image image{};
            Assert::IsTrue(ff_png_decode(span_of(sink), &arena, false, &image));

            Assert::AreEqual((int)0x80, (int)image.pixels[0]);
            Assert::AreEqual((int)0x80, (int)image.pixels[1]);
            Assert::AreEqual((int)0x80, (int)image.pixels[2]);
            Assert::AreEqual((int)0x40, (int)image.pixels[3]);

            Assert::AreEqual((int)0x20, (int)image.pixels[4]);
            Assert::AreEqual((int)0xC0, (int)image.pixels[7]);

            ff_arena_destroy(&arena);
            free(sink.data);
        }

    };
}
