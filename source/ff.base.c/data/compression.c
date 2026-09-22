#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "data/compression.h"
#include "data/stream.h"
#include <zlib/zlib.h>

static const size_t s_max_chunk_size = 1024 * 256;

// Drains everything zlib has buffered into the writer. zlib only stops producing output once it
// leaves room in the buffer, so a full buffer always means there is more to come.
static bool deflate_to_stream(z_stream* zlib, ff_stream* writer, ff_span buffer, int flush)
{
    do
    {
        zlib->avail_out = (uInt)buffer.size;
        zlib->next_out = (uint8_t*)buffer.data;

        FF_ASSERT_RET_VAL(deflate(zlib, flush) != Z_STREAM_ERROR, false);

        const ff_span written = { .data = buffer.data, .size = buffer.size - zlib->avail_out };
        FF_CHECK_RET_VAL(!written.size || ff_stream_write(writer, written), false);
    }
    while (!zlib->avail_out);

    return true;
}

bool ff_compress(ff_stream* reader, size_t full_size, ff_stream* writer)
{
    FF_ASSERT_RET_VAL(reader && writer, false);

    z_stream zlib = { 0 };
    FF_ASSERT_RET_VAL(deflateInit(&zlib, Z_BEST_COMPRESSION) == Z_OK, false);

    const size_t chunk_size = ff_math_max_size(ff_math_min_size(full_size, s_max_chunk_size), 1);
    ff_arena arena;
    ff_arena_init_heap_local(&arena, 0);

    const ff_span buffer = { .data = ff_arena_alloc(&arena, chunk_size, 1), .size = chunk_size };
    bool status = buffer.data != NULL;

    for (size_t pos = 0; status && pos < full_size; pos += chunk_size)
    {
        const ff_arena_marker marker = ff_arena_mark(&arena);
        const size_t read_size = ff_math_min_size(full_size - pos, chunk_size);

        // Reading straight from the stream avoids a separate input buffer entirely: a memory
        // stream hands back a pointer into its own data, so only a file read copies.
        const ff_span input = ff_stream_read(reader, &arena, read_size, 1);
        status = input.size == read_size;

        if (status)
        {
            zlib.avail_in = (uInt)input.size;
            zlib.next_in = (uint8_t*)input.data;

            status = deflate_to_stream(&zlib, writer, buffer, (pos + chunk_size >= full_size) ? Z_FINISH : Z_NO_FLUSH);
            FF_ASSERT(!status || !zlib.avail_in);
        }

        ff_arena_rewind(&arena, marker);
    }

    deflateEnd(&zlib);
    ff_arena_destroy(&arena);

    return status;
}

bool ff_uncompress(ff_stream* reader, size_t saved_size, ff_stream* writer)
{
    FF_ASSERT_RET_VAL(reader && writer, false);
    FF_CHECK_RET_VAL(saved_size, true);

    z_stream zlib = { 0 };
    FF_ASSERT_RET_VAL(inflateInit(&zlib) == Z_OK, false);

    const size_t chunk_size = ff_math_min_size(saved_size, s_max_chunk_size);
    ff_arena arena;
    ff_arena_init_heap_local(&arena, 0);

    const ff_span buffer = { .data = ff_arena_alloc(&arena, chunk_size * 2, 1), .size = chunk_size * 2 };
    bool status = buffer.data != NULL;
    int zlib_status = Z_OK;
    size_t pos = 0;

    for (; status && pos < saved_size; pos += chunk_size)
    {
        const ff_arena_marker marker = ff_arena_mark(&arena);
        const size_t read_size = ff_math_min_size(saved_size - pos, chunk_size);
        const ff_span input = ff_stream_read(reader, &arena, read_size, 1);
        status = input.size == read_size;

        if (status)
        {
            zlib.avail_in = (uInt)input.size;
            zlib.next_in = (uint8_t*)input.data;

            do
            {
                zlib.avail_out = (uInt)buffer.size;
                zlib.next_out = (uint8_t*)buffer.data;
                zlib_status = inflate(&zlib, Z_NO_FLUSH);
                status = zlib_status != Z_NEED_DICT && zlib_status != Z_DATA_ERROR && zlib_status != Z_MEM_ERROR && zlib_status != Z_STREAM_ERROR;

                if (status)
                {
                    const ff_span written = { .data = buffer.data, .size = buffer.size - zlib.avail_out };
                    status = !written.size || ff_stream_write(writer, written);
                }
            }
            while (status && !zlib.avail_out);
        }

        ff_arena_rewind(&arena, marker);
    }

    status = status && pos >= saved_size && zlib_status == Z_STREAM_END;

    inflateEnd(&zlib);
    ff_arena_destroy(&arena);

    return status;
}

static uint8_t base64_byte(char ch)
{
    if (ch >= 'A' && ch <= 'Z') { return (uint8_t)(ch - 'A'); }
    if (ch >= 'a' && ch <= 'z') { return (uint8_t)(ch - 'a' + 26); }
    if (ch >= '0' && ch <= '9') { return (uint8_t)(ch - '0' + 52); }
    if (ch == '+') { return 62; }
    if (ch == '/') { return 63; }

    return 0;
}

ff_span ff_decode_base64(ff_string_view text, ff_arena* arena)
{
    FF_ASSERT_RET_VAL(arena, ff_span_empty());
    FF_CHECK_RET_VAL(text.count, ff_span_empty());
    FF_ASSERT_RET_VAL(!(text.count % 4), ff_span_empty());

    size_t count = text.count / 4 * 3;
    uint8_t* out = (uint8_t*)ff_arena_alloc(arena, count, 1);
    FF_CHECK_RET_VAL(out, ff_span_empty());

    for (size_t i = 0, j = 0; i < text.count; i += 4, j += 3)
    {
        const uint8_t ch0 = base64_byte(text.data[i + 0]);
        const uint8_t ch1 = base64_byte(text.data[i + 1]);
        const uint8_t ch2 = base64_byte(text.data[i + 2]);
        const uint8_t ch3 = base64_byte(text.data[i + 3]);

        out[j + 0] = (uint8_t)((ch0 << 2) | (ch1 >> 4));
        out[j + 1] = (uint8_t)((ch1 << 4) | (ch2 >> 2));
        out[j + 2] = (uint8_t)((ch2 << 6) | ch3);
    }

    count -= (text.data[text.count - 2] == '=') ? 2 : ((text.data[text.count - 1] == '=') ? 1 : 0);

    return (ff_span)
    {
        .data = out,
        .size = count,
    };
}
