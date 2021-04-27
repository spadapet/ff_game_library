#include "pch.h"
#include "math.h"

static std::default_random_engine random_engine(std::random_device{}());
static std::uniform_int_distribution<int> random_int;

int ff::math::random_non_negative()
{
    return ::random_int(::random_engine);
}

bool ff::math::random_bool()
{
    return std::uniform_int_distribution<int>{0, 1}(::random_engine) != 0;
}

int ff::math::random_range(int start, int end)
{
    return (start != end) ? std::uniform_int_distribution<int>{start, end}(::random_engine) : start;
}

size_t ff::math::random_range(size_t start, size_t end)
{
    return (start != end) ? std::uniform_int_distribution<size_t>{start, end}(::random_engine) : start;
}

float ff::math::random_range(float start, float after_end)
{
    return (start != after_end) ? std::uniform_real_distribution<float>{start, after_end}(::random_engine) : start;
}
