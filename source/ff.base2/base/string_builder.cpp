#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/math.h"
#include "base/string_builder.h"

// Lock the layout to 4 pointer-sized fields (arena*, data, size, capacity) on x64.
static_assert(sizeof(ff::string_builder) == 32, "string_builder layout changed unexpectedly");

constexpr size_t default_initial_capacity = 1024;

// Smallest buffer worth allocating; also leaves room for the lazily-written null terminator.
constexpr size_t min_capacity = 16;

// Grow the buffer so it can hold at least 'needed' chars (which already includes the +1 reserved
// for the null terminator). Capacity at least doubles, then rounds up to a power of 2 so every
// allocation hits an allocator-friendly bucket. Re-fetches 'data' from the allocator since
// arena::realloc may relocate the block.
static bool ensure_capacity(ff::string_builder* builder, size_t needed)
{
    if (needed <= builder->capacity)
    {
        return true;
    }

    // Grow by at least doubling, but never below what's needed, then snap to a power of 2.
    size_t doubled = builder->capacity * 2;
    size_t new_capacity = ff::round_up_pow2(__max(doubled, needed));

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
    builder->size = 0;
    builder->capacity = 0;

    // Reserve room for the requested chars plus the null terminator that c_str() writes. When
    // seeding, make sure the initial content also fits so it lands in a single allocation.
    size_t wanted = __max(initial_capacity, initial.size);
    size_t needed = __max(wanted, min_capacity) + 1;
    if (::ensure_capacity(builder, needed) && initial.size)
    {
        ::memcpy(builder->data, initial.data, initial.size);
        builder->size = initial.size;
    }
}

void ff::string_builder::init(ff::arena* arena)
{
    ::init_common(this, arena, default_initial_capacity, ff::string_view{ nullptr, 0 });
}

void ff::string_builder::init(ff::arena* arena, size_t initial_capacity)
{
    ::init_common(this, arena, initial_capacity, ff::string_view{ nullptr, 0 });
}

void ff::string_builder::init(ff::arena* arena, ff::string_view initial)
{
    ::init_common(this, arena, default_initial_capacity, initial);
}

ff::string_builder* ff::string_builder::reset()
{
    this->size = 0;
    return this;
}

ff::string_builder* ff::string_builder::reserve(size_t capacity)
{
    // +1 keeps room for the null terminator even when the caller reserves an exact content size.
    FF_VERIFY(::ensure_capacity(this, capacity + 1));
    return this;
}

ff::string_builder* ff::string_builder::append(char value)
{
    if (::ensure_capacity(this, this->size + 2)) // +1 char, +1 terminator
    {
        this->data[this->size++] = value;
    }

    return this;
}

ff::string_builder* ff::string_builder::append(ff::string_view value)
{
    if (value.size && ::ensure_capacity(this, this->size + value.size + 1))
    {
        ::memcpy(this->data + this->size, value.data, value.size);
        this->size += value.size;
    }

    return this;
}

ff::string_builder* ff::string_builder::insert(size_t pos, char value)
{
    return this->insert(pos, ff::string_view{ &value, 1 });
}

ff::string_builder* ff::string_builder::insert(size_t pos, ff::string_view value)
{
    FF_ASSERT_RET_VAL(pos <= this->size, this);

    if (value.size && ::ensure_capacity(this, this->size + value.size + 1))
    {
        // Shift the tail right to open a gap, then copy the new chars in. memmove handles overlap.
        ::memmove(this->data + pos + value.size, this->data + pos, this->size - pos);
        ::memcpy(this->data + pos, value.data, value.size);
        this->size += value.size;
    }

    return this;
}

ff::string_builder* ff::string_builder::remove(size_t pos, size_t count)
{
    FF_ASSERT_RET_VAL(pos <= this->size, this);

    // Clamp so removing past the end just trims to the end instead of underflowing.
    count = __min(count, this->size - pos);
    if (count)
    {
        // Slide the tail left over the removed range. memmove handles the overlap.
        ::memmove(this->data + pos, this->data + pos + count, this->size - pos - count);
        this->size -= count;
    }

    return this;
}

const char* ff::string_builder::c_str() const
{
    // capacity is always >= size + 1 (init/append/insert reserve the terminator slot), so this
    // write never overflows. const is fine: the terminator slot is reserved, so writing it doesn't
    // change the logical content or any member (only bytes the data pointer already owns).
    this->data[this->size] = '\0';
    return this->data;
}

ff::string_view ff::string_builder::view() const
{
    ff::string_view result;
    result.data = this->data;
    result.size = this->size;
    return result;
}
