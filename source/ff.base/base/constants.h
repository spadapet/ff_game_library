#pragma once

/// <summary>
/// Useful global constants
/// </summary>
namespace ff::constants
{
    constexpr bool debug_build = static_cast<bool>(DEBUG);
    constexpr bool profile_build = static_cast<bool>(PROFILE_APP);
    constexpr size_t bits_build = sizeof(size_t) * 8;

#ifdef TRACK_MEMORY_ALLOCATIONS
    constexpr bool track_memory = true;
#else
    constexpr bool track_memory = false;
#endif

    template<class T, class = std::enable_if_t<std::is_unsigned_v<T>>>
    constexpr T invalid_unsigned()
    {
        return static_cast<T>(-1);
    }

    template<class T, class = std::enable_if_t<std::is_unsigned_v<T>>>
    constexpr T previous_unsigned(T val)
    {
        return val ? (val - 1) : ff::constants::invalid_unsigned<T>();
    }

    /// <summary>
    /// Fixed amount of times per second that the game loop advances
    /// </summary>
    template<class T, class = std::enable_if_t<std::is_arithmetic_v<T>>>
    constexpr T advances_per_second()
    {
        return static_cast<T>(60.0);
    }

    /// <summary>
    /// Each time the game advances, this fixed amount of time has passed
    /// </summary>
    template<class T, class = std::enable_if_t<std::is_arithmetic_v<T>>>
    constexpr T seconds_per_advance()
    {
        return static_cast<T>(1.0) / ff::constants::advances_per_second<T>();
    }
}
