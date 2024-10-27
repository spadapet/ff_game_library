#include "pch.h"
#include "base/math.h"

static std::default_random_engine& random_engine()
{
    static std::default_random_engine value(std::random_device{}());
    return value;
}

static std::uniform_int_distribution<int>& random_int()
{
    static std::uniform_int_distribution<int> value;
    return value;
}

uint8_t* ff::math::align_down(uint8_t* data, size_t align) noexcept
{
    return reinterpret_cast<uint8_t*>(ff::math::align_down(reinterpret_cast<size_t>(data), align));
}

uint8_t* ff::math::align_up(uint8_t* data, size_t align) noexcept
{
    return reinterpret_cast<uint8_t*>(ff::math::align_up(reinterpret_cast<size_t>(data), align));
}

int ff::math::random_non_negative()
{
    return ::random_int()(::random_engine());
}

bool ff::math::random_bool()
{
    return std::uniform_int_distribution<int>{0, 1}(::random_engine()) != 0;
}

int ff::math::random_range(int start, int end)
{
    return (start != end) ? std::uniform_int_distribution<int>{start, end}(::random_engine()) : start;
}

size_t ff::math::random_range(size_t start, size_t end)
{
    return (start != end) ? std::uniform_int_distribution<size_t>{start, end}(::random_engine()) : start;
}

float ff::math::random_range(float start, float after_end)
{
    return (start != after_end) ? std::uniform_real_distribution<float>{start, after_end}(::random_engine()) : start;
}
