#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/dict.h"

void ff::dict::init(ff::arena* arena)
{
}

void ff::dict::init(ff::arena* arena, size_t initial_capacity)
{
}

void ff::dict::init(ff::arena* arena, const ff::dict& other)
{
}

void ff::dict::set(ff::string_view key, const ff::value& value)
{
}

ff::value* ff::dict::get(ff::string_view key) const
{
    return nullptr;
}

bool ff::dict::clear(ff::string_view key)
{
    return false;
}

void ff::dict::reset()
{
}

void ff::dict::pack(ff::arena* arena, ff::idict& dest) const
{
}
