#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string_builder.h"

static const size_t s_default_initial_capacity = 1024;
static const size_t s_min_capacity = 16;

static bool ensure_capacity(ff_string_builder* builder, size_t needed)
{
    if (needed <= builder->capacity)
    {
        return true;
    }

    size_t doubled = builder->capacity * 2;
    size_t new_capacity = ff_math_round_up_pow2(ff_math_max_size(doubled, needed));

    // arena::realloc may relocate the block, so re-fetch 'data'.
    char* new_data = (char*)ff_arena_realloc(builder->arena, builder->data, builder->capacity, new_capacity, 1);
    FF_ASSERT_RET_VAL(new_data, false);

    builder->data = new_data;
    builder->capacity = new_capacity;
    return true;
}

static void init_common(ff_string_builder* builder, ff_arena* arena, size_t initial_capacity, ff_string_view initial)
{
    builder->arena = arena;
    builder->data = NULL;
    builder->count = 0;
    builder->capacity = 0;

    // Size for the requested capacity (and any seed content) in a single allocation; no terminator slot.
    size_t wanted = ff_math_max_size(initial_capacity, initial.count);
    size_t needed = ff_math_max_size(wanted, s_min_capacity);
    if (ensure_capacity(builder, needed) && initial.count && builder->data)
    {
        memcpy(builder->data, initial.data, initial.count);
        builder->count = initial.count;
    }
}

void ff_string_builder_init(ff_string_builder* sb, ff_arena* arena)
{
    size_t arena_capacity = (arena->end - arena->next);
    size_t initial_capacity = arena_capacity
        ? ff_math_min_size(arena_capacity, s_default_initial_capacity)
        : s_default_initial_capacity;

    ff_string_builder_init_capacity(sb, arena, initial_capacity);
}

void ff_string_builder_init_capacity(ff_string_builder* sb, ff_arena* arena, size_t initial_capacity)
{
    ff_string_view empty = ff_string_view_empty();
    init_common(sb, arena, initial_capacity, empty);
}

void ff_string_builder_init_string(ff_string_builder* sb, ff_arena* arena, ff_string_view initial)
{
    init_common(sb, arena, s_default_initial_capacity, initial);
}

void ff_string_builder_reset(ff_string_builder* sb)
{
    sb->count = 0;
}

void ff_string_builder_reserve(ff_string_builder* sb, size_t capacity)
{
    FF_VERIFY(ensure_capacity(sb, capacity));
}

void ff_string_builder_append_char(ff_string_builder* sb, char value)
{
    if (ensure_capacity(sb, sb->count + 1))
    {
        sb->data[sb->count++] = value;
    }
}

void ff_string_builder_append(ff_string_builder* sb, ff_string_view value)
{
    if (value.count && ensure_capacity(sb, sb->count + value.count))
    {
        memcpy(sb->data + sb->count, value.data, value.count);
        sb->count += value.count;
    }
}

void ff_string_builder_append_format_v(ff_string_builder* sb, ff_string_view format, va_list args)
{
    // The CRT formatters need a null-terminated format, but our format is a (maybe non-terminated)
    // string_view. Copy it into a throwaway arena backed by a stack buffer (spills to the heap only
    // if the format exceeds it). A separate arena keeps the copy independent of the output buffer.
    char format_stack[1024];
    ff_arena temp_arena;
    ff_arena_init_external(&temp_arena, format_stack, sizeof(format_stack), 0);

    char* format_copy = (char*)ff_arena_alloc(&temp_arena, format.count + 1, 1);
    if (!format_copy)
    {
        FF_ASSERT(format_copy);
        ff_arena_destroy(&temp_arena);
        return;
    }

    memcpy(format_copy, format.data, format.count);
    format_copy[format.count] = '\0';

    // va_copy: the measure pass consumes the va_list, leaving 'args' for the fill pass.
    va_list measure_args;
    va_copy(measure_args, args);
    int needed = _vscprintf(format_copy, measure_args);
    va_end(measure_args);

    // vsnprintf always writes a trailing '\0', so it needs 'needed + 1' bytes even though we don't
    // keep the terminator (count advances by 'needed' only).
    if (needed > 0 && ensure_capacity(sb, sb->count + (size_t)needed + 1) && sb->data)
    {
        vsnprintf(sb->data + sb->count, (size_t)needed + 1, format_copy, args);
        sb->count += (size_t)needed;
    }

    ff_arena_destroy(&temp_arena);
}

void ff_string_builder_append_format(ff_string_builder* sb, ff_string_view format, ...)
{
    va_list args;
    va_start(args, format);
    ff_string_builder_append_format_v(sb, format, args);
    va_end(args);
}

void ff_string_builder_insert_char(ff_string_builder* sb, size_t pos, char value)
{
    ff_string_view single = (ff_string_view)
    {
        .data = &value,
        .count = 1
    };

    ff_string_builder_insert(sb, pos, single);
}

void ff_string_builder_insert(ff_string_builder* sb, size_t pos, ff_string_view value)
{
    FF_ASSERT_RET(pos <= sb->count);

    if (value.count && ensure_capacity(sb, sb->count + value.count))
    {
        memmove(sb->data + pos + value.count, sb->data + pos, sb->count - pos);
        memcpy(sb->data + pos, value.data, value.count);
        sb->count += value.count;
    }
}

void ff_string_builder_remove(ff_string_builder* sb, size_t pos, size_t count)
{
    FF_ASSERT_RET(pos <= sb->count);

    count = ff_math_min_size(count, sb->count - pos); // clamp so removing past the end trims to the end
    if (count)
    {
        memmove(sb->data + pos, sb->data + pos + count, sb->count - pos - count);
        sb->count -= count;
    }
}

ff_string_view ff_string_builder_view(const ff_string_builder* sb)
{
    return (ff_string_view)
    {
        .data = sb->data,
        .count = sb->count
    };
}

ff_string_view ff_string_builder_copy(const ff_string_builder* sb)
{
    return ff_string_builder_copy_to(sb, sb->arena);
}

ff_string_view ff_string_builder_copy_to(const ff_string_builder* sb, ff_arena* arena)
{
    char* dest = (char*)ff_arena_alloc(arena, sb->count + 1, alignof(char));
    FF_ASSERT_RET_VAL(dest, ff_string_view_empty());

    if (sb->count)
    {
        memcpy(dest, sb->data, sb->count);
    }

    dest[sb->count] = '\0';

	return (ff_string_view)
	{
		.data = dest,
		.count = sb->count
	};
}
