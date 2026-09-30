#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string.h"
#include "data/stream.h"

static const size_t s_min_write_capacity = 64;

static bool is_read(const ff_stream* stream)
{
    return stream->type == ff_stream_type_read_file || stream->type == ff_stream_type_read_memory;
}

static bool is_path_separator(wchar_t ch)
{
    return ch == L'\\' || ch == L'/';
}

static void create_parent_directories(wchar_t* path, size_t count)
{
    size_t i = (count >= 2 && path[1] == L':') ? 2 : 0;

    while (i < count && is_path_separator(path[i]))
    {
        i++;
    }

    for (; i < count; i++)
    {
        if (is_path_separator(path[i]))
        {
            wchar_t separator = path[i];
            path[i] = 0;
            BOOL result = CreateDirectory(path, NULL);
            path[i] = separator;

            FF_CHECK_RET(result || GetLastError() == ERROR_ALREADY_EXISTS);
        }
    }
}

static HANDLE open_file(ff_string_view path, bool write)
{
    ff_arena_declare_stack(temp_arena, 1024 * sizeof(wchar_t));
    ff_wstring_view wide_path = ff_utf8_to_wide(path, &temp_arena, true);
    HANDLE file = NULL;

    if (wide_path.count)
    {
        if (write)
        {
            create_parent_directories((wchar_t*)wide_path.data, wide_path.count);
        }

        file = write
            ? CreateFile(wide_path.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
            : CreateFile(wide_path.data, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

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
    FF_CHECK_RET_VAL(doubled >= stream->capacity, false);

    size_t wanted = ff_math_max_size(ff_math_max_size(doubled, needed), s_min_write_capacity);
    size_t new_capacity = ff_math_round_up_pow2(wanted);
    FF_CHECK_RET_VAL(new_capacity >= wanted, false);

    uint8_t* data = (uint8_t*)ff_arena_realloc(stream->arena, stream->data, stream->capacity, new_capacity, 1);
    FF_CHECK_RET_VAL(data, false);

    stream->data = data;
    stream->capacity = new_capacity;
    return true;
}

ff_stream ff_stream_none(void)
{
    return (ff_stream){ .type = ff_stream_type_none };
}

bool ff_stream_init_read_file(ff_stream* stream, ff_string_view path)
{
    *stream = ff_stream_none();
    HANDLE file = open_file(path, false);
    FF_CHECK_RET_VAL(file, false);

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0 || (uint64_t)file_size.QuadPart > (uint64_t)SIZE_MAX)
    {
        CloseHandle(file);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    *stream = (ff_stream)
    {
        .type = ff_stream_type_read_file,
        .file = file,
        .size = (size_t)file_size.QuadPart,
    };

    return true;
}

bool ff_stream_init_write_file(ff_stream* stream, ff_string_view path)
{
    *stream = ff_stream_none();
    HANDLE file = open_file(path, true);
    FF_CHECK_RET_VAL(file, false);

    *stream = (ff_stream){ .type = ff_stream_type_write_file, .file = file };
    return true;
}

void ff_stream_init_read_memory(ff_stream* stream, ff_span span)
{
    *stream = (ff_stream)
    {
        .type = ff_stream_type_read_memory,
        .data = (uint8_t*)span.data,
        .size = span.data ? span.size : 0,
    };
}

void ff_stream_init_write_memory(ff_stream* stream, ff_arena* arena, size_t initial_capacity)
{
    *stream = (ff_stream)
    {
        .type = ff_stream_type_write_memory,
        .arena = arena,
    };

    if (initial_capacity && !ensure_write_capacity(stream, initial_capacity))
    {
        *stream = ff_stream_none();
    }
}

ff_span ff_stream_written(const ff_stream* stream)
{
    FF_ASSERT_RET_VAL(stream->type == ff_stream_type_write_memory, ff_span_empty());
    FF_CHECK_RET_VAL(stream->size, ff_span_empty());

    return (ff_span)
    {
        .data = stream->data,
        .size = stream->size,
    };
}

void ff_stream_destroy(ff_stream* stream)
{
    if (stream->file)
    {
        CloseHandle(stream->file);
    }

    *stream = ff_stream_none();
}

ff_span ff_stream_read(ff_stream* stream, ff_arena* arena, size_t size, size_t align)
{
    FF_ASSERT_RET_VAL(is_read(stream), ff_span_empty());
    FF_ASSERT_RET_VAL(align && ff_math_is_pow2(align), ff_span_empty());
    size = ff_math_min_size(size, stream->size - stream->pos);
    FF_CHECK_RET_VAL(size, ff_span_empty());

    if (stream->type == ff_stream_type_read_memory)
    {
        const uint8_t* source = stream->data + stream->pos;
        stream->pos += size;

        if (!((uintptr_t)source & (align - 1)))
        {
            return (ff_span)
            {
                .data = source,
                .size = size,
            };
        }

        FF_ASSERT_RET_VAL(arena, ff_span_empty());

        void* copy = ff_arena_alloc(arena, size, align);
        FF_CHECK_RET_VAL(copy, ff_span_empty());
        memcpy(copy, source, size);

        return (ff_span)
        {
            .data = copy,
            .size = size,
        };
    }

    uint8_t* dest = (uint8_t*)ff_arena_alloc(arena, size, align);
    FF_CHECK_RET_VAL(dest, ff_span_empty());

    size_t total_read = 0;
    while (total_read < size)
    {
        size_t remaining = size - total_read;
        DWORD to_read = (DWORD)ff_math_min_size(remaining, UINT32_MAX);
        DWORD read = 0;

        if (!ReadFile(stream->file, dest + total_read, to_read, &read, NULL))
        {
            FF_DEBUG_FAIL_RET_VAL(ff_span_empty());
        }

        total_read += read;

        if (!read)
        {
            break;
        }
    }

    stream->pos += total_read;
    return (ff_span)
    {
        .data = dest,
        .size = total_read,
    };
}

bool ff_stream_write(ff_stream* stream, ff_span data)
{
    FF_ASSERT_RET_VAL(stream->type == ff_stream_type_write_file || stream->type == ff_stream_type_write_memory, false);
    FF_CHECK_RET_VAL(data.size, true);
    FF_ASSERT_RET_VAL(data.data, false);
    FF_CHECK_RET_VAL(data.size <= SIZE_MAX - stream->size, false);

    if (stream->type == ff_stream_type_write_memory)
    {
        FF_CHECK_RET_VAL(ensure_write_capacity(stream, stream->size + data.size), false);
        memcpy(stream->data + stream->size, data.data, data.size);
        stream->size += data.size;
        return true;
    }

    const uint8_t* bytes = (const uint8_t*)data.data;
    size_t total_written = 0;
    while (total_written < data.size)
    {
        size_t remaining = data.size - total_written;
        DWORD to_write = (DWORD)ff_math_min_size(remaining, UINT32_MAX);
        DWORD written = 0;

        if (!WriteFile(stream->file, bytes + total_written, to_write, &written, NULL) || !written)
        {
            FF_DEBUG_FAIL_RET_VAL(false);
        }

        total_written += written;
    }

    stream->size += total_written;
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
