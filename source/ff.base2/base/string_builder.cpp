#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
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

    // +1 reserves the null terminator slot; seed content must also fit for a single allocation.
    size_t wanted = __max(initial_capacity, initial.count);
    size_t needed = __max(wanted, ::min_capacity) + 1;
    if (::ensure_capacity(builder, needed) && initial.count && builder->data)
    {
        ::memcpy(builder->data, initial.data, initial.count);
        builder->count = initial.count;
    }
}

void ff::string_builder::init(ff::arena* arena)
{
    ::init_common(this, arena, ::default_initial_capacity, ff::string_view{ nullptr, 0 });
}

void ff::string_builder::init(ff::arena* arena, size_t initial_capacity)
{
    ::init_common(this, arena, initial_capacity, ff::string_view{ nullptr, 0 });
}

void ff::string_builder::init(ff::arena* arena, ff::string_view initial)
{
    ::init_common(this, arena, ::default_initial_capacity, initial);
}

void ff::string_builder::init_format(ff::arena* arena, ff::string_view format, ...)
{
    va_list args;
    va_start(args, format);
    this->init_format_v(arena, format, args);
    va_end(args);
}

void ff::string_builder::init_format_v(ff::arena* arena, ff::string_view format, va_list args)
{
    // Init with minimal capacity so append_format_v sizes the buffer in a single allocation rather
    // than pre-allocating the default capacity and then growing past it.
    this->init(arena, (size_t)0);
    this->append_format_v(format, args);
}

ff::string_builder* ff::string_builder::reset()
{
    this->count = 0;
    return this;
}

ff::string_builder* ff::string_builder::reserve(size_t capacity)
{
    FF_VERIFY(::ensure_capacity(this, capacity + 1)); // +1 for the null terminator slot
    return this;
}

ff::string_builder* ff::string_builder::append(char value)
{
    if (::ensure_capacity(this, this->count + 2))
    {
        this->data[this->count++] = value;
    }

    return this;
}

ff::string_builder* ff::string_builder::append(ff::string_view value)
{
    if (value.count && ::ensure_capacity(this, this->count + value.count + 1))
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

    if (value.count && ::ensure_capacity(this, this->count + value.count + 1))
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

const char* ff::string_builder::c_str() const
{
    // const is safe: capacity always reserves the terminator slot, so writing it changes no member.
    this->data[this->count] = '\0';
    return this->data;
}

ff::string_view ff::string_builder::view() const
{
    ff::string_view result;
    result.data = this->data;
    result.count = this->count;
    return result;
}

const char* ff::string_builder::store(ff::arena* arena) const
{
    // Always return an allocated array so ff::array_size works (empty included); null only on alloc failure.
    char* result = ff::array_init<char>(arena ? arena : this->arena, this->count + 1);
    FF_ASSERT_RET_VAL(result, nullptr);

    ff::array_resize(result, this->count);

    if (this->count)
    {
        ::memcpy(result, this->data, this->count);
    }

    result[this->count] = '\0';
    return result;
}
