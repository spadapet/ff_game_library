#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string_builder.h"

static_assert(sizeof(ff::string_builder) == 32, "string_builder layout changed unexpectedly");

constexpr size_t default_initial_capacity = 1024;
constexpr size_t min_capacity = 16;

static bool ensure_capacity(ff::string_builder* builder, size_t needed)
{
    if (needed <= builder->capacity)
    {
        return true;
    }

    size_t doubled = builder->capacity * 2;
    size_t new_capacity = ff::round_up_pow2(__max(doubled, needed));

    // arena::realloc may relocate the block, so re-fetch 'data'.
    char* new_data = (char*)builder->arena->realloc(builder->data, builder->capacity, new_capacity, 1);
    FF_ASSERT_RET_VAL(new_data, false);

    builder->data = new_data;
    builder->capacity = new_capacity;
    return true;
}

static void init_common(ff::string_builder* builder, ff::arena* arena, size_t initial_capacity, ff::string_view initial)
{
    builder->arena = arena;
    builder->data = nullptr;
    builder->count = 0;
    builder->capacity = 0;

    // Size for the requested capacity (and any seed content) in a single allocation; no terminator slot.
    size_t wanted = __max(initial_capacity, initial.count);
    size_t needed = __max(wanted, ::min_capacity);
    if (::ensure_capacity(builder, needed) && initial.count && builder->data)
    {
        ::memcpy(builder->data, initial.data, initial.count);
        builder->count = initial.count;
    }
}

void ff::string_builder::init(ff::arena* arena)
{
    size_t arena_capacity = (arena->end - arena->next);
    size_t initial_capacity = arena_capacity
        ? __min(arena_capacity, ::default_initial_capacity)
        : ::default_initial_capacity;

    this->init(arena, initial_capacity);
}

void ff::string_builder::init(ff::arena* arena, size_t initial_capacity)
{
    ::init_common(this, arena, initial_capacity, ff::string_view{ nullptr, 0 });
}

void ff::string_builder::init(ff::arena* arena, ff::string_view initial)
{
    ::init_common(this, arena, ::default_initial_capacity, initial);
}

ff::string_builder* ff::string_builder::reset()
{
    this->count = 0;
    return this;
}

ff::string_builder* ff::string_builder::reserve(size_t capacity)
{
    FF_VERIFY(::ensure_capacity(this, capacity));
    return this;
}

ff::string_builder* ff::string_builder::append(char value)
{
    if (::ensure_capacity(this, this->count + 1))
    {
        this->data[this->count++] = value;
    }

    return this;
}

ff::string_builder* ff::string_builder::append(ff::string_view value)
{
    if (value.count && ::ensure_capacity(this, this->count + value.count))
    {
        ::memcpy(this->data + this->count, value.data, value.count);
        this->count += value.count;
    }

    return this;
}

ff::string_builder* ff::string_builder::append_format_v(ff::string_view format, va_list args)
{
    // The CRT formatters need a null-terminated format, but our format is a (maybe non-terminated)
    // string_view. Copy it into a throwaway arena backed by a stack buffer (spills to the heap only
    // if the format exceeds it). A separate arena keeps the copy independent of the output buffer.
    char format_stack[1024];
    ff::arena temp_arena;
    temp_arena.init_external(format_stack, sizeof(format_stack), 0);

    char* format_copy = (char*)temp_arena.alloc(format.count + 1, 1);
    if (!format_copy)
    {
        FF_ASSERT(format_copy);
        temp_arena.destroy();
        return this;
    }

    ::memcpy(format_copy, format.data, format.count);
    format_copy[format.count] = '\0';

    // va_copy: the measure pass consumes the va_list, leaving 'args' for the fill pass.
    va_list measure_args;
    va_copy(measure_args, args);
    int needed = ::_vscprintf(format_copy, measure_args);
    va_end(measure_args);

    // vsnprintf always writes a trailing '\0', so it needs 'needed + 1' bytes even though we don't
    // keep the terminator (count advances by 'needed' only).
    if (needed > 0 && ::ensure_capacity(this, this->count + (size_t)needed + 1) && this->data)
    {
        ::vsnprintf(this->data + this->count, (size_t)needed + 1, format_copy, args);
        this->count += (size_t)needed;
    }

    temp_arena.destroy();
    return this;
}

ff::string_builder* ff::string_builder::append_format(ff::string_view format, ...)
{
    va_list args;
    va_start(args, format);
    this->append_format_v(format, args);
    va_end(args);
    return this;
}

ff::string_builder* ff::string_builder::insert(size_t pos, char value)
{
    return this->insert(pos, ff::string_view{ &value, 1 });
}

ff::string_builder* ff::string_builder::insert(size_t pos, ff::string_view value)
{
    FF_ASSERT_RET_VAL(pos <= this->count, this);

    if (value.count && ::ensure_capacity(this, this->count + value.count))
    {
        ::memmove(this->data + pos + value.count, this->data + pos, this->count - pos);
        ::memcpy(this->data + pos, value.data, value.count);
        this->count += value.count;
    }

    return this;
}

ff::string_builder* ff::string_builder::remove(size_t pos, size_t count)
{
    FF_ASSERT_RET_VAL(pos <= this->count, this);

    count = __min(count, this->count - pos); // clamp so removing past the end trims to the end
    if (count)
    {
        ::memmove(this->data + pos, this->data + pos + count, this->count - pos - count);
        this->count -= count;
    }

    return this;
}

ff::string_view ff::string_builder::view() const
{
    ff::string_view result;
    result.data = this->data;
    result.count = this->count;
    return result;
}

ff::string_view ff::string_builder::copy(ff::arena* arena) const
{
    // One extra byte holds a '\0' so 'data' works as a C-string; the returned 'count' excludes it.
    // A static empty (still null-terminated) string is returned only on alloc failure.
    ff::string_view result{ "", 0 };

    char* dest = (char*)(arena ? arena : this->arena)->alloc(this->count + 1, alignof(char));
    FF_ASSERT_RET_VAL(dest, result);

    if (this->count)
    {
        ::memcpy(dest, this->data, this->count);
    }

    dest[this->count] = '\0';

    result.data = dest;
    result.count = this->count;
    return result;
}
