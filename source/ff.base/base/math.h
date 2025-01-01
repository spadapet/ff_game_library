#pragma once

#include "../base/assert.h"
#include "../types/fixed.h"

/// <summary>
/// Useful math helper functions
/// </summary>
namespace ff::math
{
    /// <summary>
    /// Convert radians to degrees
    /// </summary>
    /// <typeparam name="T">Type to return</typeparam>
    template<class T>
    constexpr T radians_to_degrees(T angle)
    {
        return angle * static_cast<T>(180.0) / std::numbers::pi_v<T>;
    }

    /// <summary>
    /// Convert degrees to radians
    /// </summary>
    /// <typeparam name="T">Type to return</typeparam>
    template<class T>
    constexpr T degrees_to_radians(T angle)
    {
        return angle * std::numbers::pi_v<T> / static_cast<T>(180.0);
    }

    template<class T>
    constexpr T nearest_power_of_two(T num) noexcept
    {
        if (num < 1)
        {
            return 1;
        }

        return std::bit_ceil(static_cast<std::make_unsigned_t<T>>(num));
    }

    template<typename T>
    constexpr bool is_power_of_2(T num) noexcept
    {
        if (num < 1)
        {
            return false;
        }

        return std::has_single_bit(static_cast<std::make_unsigned_t<T>>(num));
    }

    template<typename T>
    constexpr T align_down(T size, T align) noexcept
    {
        align += (align == 0);
        assert(ff::math::is_power_of_2(align));
        return size & ~(align - 1);
    }

    template<typename T>
    constexpr T align_up(T size, T align) noexcept
    {
        align += (align == 0);
        assert(ff::math::is_power_of_2(align));
        T mask = (align - 1);
        return (size + mask) & ~mask;
    }

    uint8_t* align_down(uint8_t* data, size_t align) noexcept;
    uint8_t* align_up(uint8_t* data, size_t align) noexcept;

    template<class T>
    constexpr T round_up(T value, T multiple)
    {
        return (value + multiple - static_cast<T>(1)) / multiple * multiple;
    }

    int random_non_negative();
    bool random_bool();
    int random_range(int start, int end);
    size_t random_range(size_t start, size_t end);
    float random_range(float start, float after_end);

    template<class T, class ExpandedT, T FixedCount>
    ff::fixed_t<T, ExpandedT, FixedCount> random_range(ff::fixed_t<T, ExpandedT, FixedCount> start, ff::fixed_t<T, ExpandedT, FixedCount> after_end)
    {
        using f_t = typename ff::fixed_t<T, ExpandedT, FixedCount>;
        return (start != after_end) ? f_t::from_raw(ff::math::random_range(start.get_raw(), after_end.get_raw() - 1)) : start;
    }

    template<typename T>
    T random_range(const std::pair<T, T>& range)
    {
        return ff::math::random_range(range.first, range.second);
    }
}
