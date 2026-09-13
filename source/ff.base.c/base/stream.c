#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/stream.h"
#include "base/string.h"

static const size_t s_min_write_capacity = 64;

static void init_common(ff_stream* stream, ff_stream_type type)
{
    *stream = (ff_stream){ .type = type };
}

static bool is_read(const ff_stream* stream)
{
    return stream->type == ff_stream_type_read_file || stream->type == ff_stream_type_read_memory;
}

static HANDLE open_file(ff_string_view path, bool write)
{
    wchar_t path_stack[1024];
    ff_arena temp_arena;
    ff_arena_init_external(&temp_arena, path_stack, sizeof(path_stack), 0);

    ff_wstring_view wide_path = ff_utf8_to_wide(path, &temp_arena);
    HANDLE file = NULL;

    if (wide_path.count)
    {
        file = write
            ? CreateFileW(wide_path.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
            : CreateFileW(wide_path.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file == INVALID_HANDLE_VALUE)
        {
            file = NULL;
        }
    }

    ff_arena_destroy(&temp_arena);
    return file;
}

static bool ensure_write_capacity(ff_stream* stream, size_t needed)
{
    if (needed <= stream->capacity)
    {
        return true;
    }

    size_t doubled = stream->capacity * 2;
    size_t new_capacity = ff_math_round_up_pow2(ff_math_max_size(ff_math_max_size(doubled, needed), s_min_write_capacity));

    uint8_t* new_data = (uint8_t*)ff_arena_realloc(stream->arena, stream->data, stream->capacity, new_capacity, 1);
    FF_ASSERT_RET_VAL(new_data, false);

    stream->data = new_data;
    stream->capacity = new_capacity;
    return true;
}

bool ff_stream_init_read_file(ff_stream* stream, ff_string_view path)
{
    FF_ASSERT_RET_VAL(stream, false);
    init_common(stream, ff_stream_type_none);

    HANDLE file = open_file(path, false);
    FF_CHECK_RET_VAL(file, false);

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0)
    {
        CloseHandle(file);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    init_common(stream, ff_stream_type_read_file);
    stream->file = file;
    stream->size = (size_t)file_size.QuadPart;
    return true;
}

bool ff_stream_init_write_file(ff_stream* stream, ff_string_view path)
{
    FF_ASSERT_RET_VAL(stream, false);
    init_common(stream, ff_stream_type_none);

    HANDLE file = open_file(path, true);
    FF_CHECK_RET_VAL(file, false);

    init_common(stream, ff_stream_type_write_file);
    stream->file = file;
    return true;
}

void ff_stream_init_read_memory(ff_stream* stream, ff_span span)
{
    FF_ASSERT_RET(stream);

    init_common(stream, ff_stream_type_read_memory);
    stream->data = (uint8_t*)span.data;
    stream->size = span.data ? span.size : 0;
}

bool ff_stream_init_write_memory(ff_stream* stream, ff_arena* arena, size_t initial_capacity)
{
    FF_ASSERT_RET_VAL(stream, false);
    init_common(stream, ff_stream_type_none);
    FF_ASSERT_RET_VAL(arena, false);

    init_common(stream, ff_stream_type_write_memory);
    stream->arena = arena;

    if (initial_capacity && !ensure_write_capacity(stream, initial_capacity))
    {
        init_common(stream, ff_stream_type_none);
        return false;
    }

    return true;
}

ff_span ff_stream_written(const ff_stream* stream)
{
    ff_span result = (ff_span){ 0 };
    FF_ASSERT_RET_VAL(stream, result);
    FF_ASSERT_RET_VAL(stream->type == ff_stream_type_write_memory, result);
    FF_CHECK_RET_VAL(stream->size, result);

    result.data = stream->data;
    result.size = stream->size;
    return result;
}

void ff_stream_destroy(ff_stream* stream)
{
    FF_ASSERT_RET(stream);

    if (stream->file)
    {
        CloseHandle(stream->file);
    }

    init_common(stream, ff_stream_type_none);
}

ff_span ff_stream_read(ff_stream* stream, ff_arena* arena, size_t size)
{
    ff_span result = (ff_span){ 0 };
    FF_ASSERT_RET_VAL(stream, result);
    FF_ASSERT_RET_VAL(is_read(stream), result);

    size = ff_math_min_size(size, stream->size - stream->pos);
    FF_CHECK_RET_VAL(size, result);

    if (stream->type == ff_stream_type_read_memory)
    {
        result.data = stream->data + stream->pos;
        result.size = size;
        stream->pos += size;
        return result;
    }

    FF_ASSERT_RET_VAL(arena, result);
    FF_ASSERT_RET_VAL(size <= MAXDWORD, result);

    uint8_t* dest = ff_arena_alloc_type(arena, uint8_t, size);
    FF_ASSERT_RET_VAL(dest, result);

    DWORD read = 0;
    if (!ReadFile(stream->file, dest, (DWORD)size, &read, NULL))
    {
        FF_DEBUG_FAIL_RET_VAL(result);
    }

    stream->pos += read;
    result.data = dest;
    result.size = read;
    return result;
}

bool ff_stream_write(ff_stream* stream, ff_span data)
{
    FF_ASSERT_RET_VAL(stream, false);
    FF_ASSERT_RET_VAL(stream->type == ff_stream_type_write_file || stream->type == ff_stream_type_write_memory, false);
    FF_CHECK_RET_VAL(data.size, true);
    FF_ASSERT_RET_VAL(data.data, false);

    if (stream->type == ff_stream_type_write_memory)
    {
        FF_CHECK_RET_VAL(ensure_write_capacity(stream, stream->size + data.size), false);

        memcpy(stream->data + stream->size, data.data, data.size);
        stream->size += data.size;
        return true;
    }

    FF_ASSERT_RET_VAL(data.size <= MAXDWORD, false);

    DWORD written = 0;
    if (!WriteFile(stream->file, data.data, (DWORD)data.size, &written, NULL) || written != data.size)
    {
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    stream->size += written;
    return true;
}

size_t ff_stream_size(const ff_stream* stream)
{
    FF_ASSERT_RET_VAL(stream, 0);
    return stream->size;
}

size_t ff_stream_pos(const ff_stream* stream)
{
    FF_ASSERT_RET_VAL(stream, 0);
    return is_read(stream) ? stream->pos : stream->size;
}

bool ff_stream_seek(ff_stream* stream, size_t pos)
{
    FF_ASSERT_RET_VAL(stream, false);
    FF_ASSERT_RET_VAL(is_read(stream), false);

    pos = ff_math_min_size(pos, stream->size);

    if (stream->type == ff_stream_type_read_file)
    {
        LARGE_INTEGER move;
        move.QuadPart = (LONGLONG)pos;
        FF_ASSERT_RET_VAL(SetFilePointerEx(stream->file, move, NULL, FILE_BEGIN), false);
    }

    stream->pos = pos;
    return true;
}
