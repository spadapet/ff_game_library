#pragma once

/// <summary>
/// Useful global constants
/// </summary>
namespace ff::constants
{
    constexpr bool debug_build = static_cast<bool>(DEBUG);
    constexpr size_t bits_build = sizeof(size_t) * 8;
    constexpr size_t invalid_size = static_cast<size_t>(-1);
    constexpr DWORD invalid_dword = static_cast<DWORD>(-1);

    /// <summary>
    /// Fixed amount of times per second that the game loop advances
    /// </summary>
    constexpr double advances_per_second = 60.0;
    constexpr float advances_per_second_f = 60.0f;
    constexpr size_t advances_per_second_s = 60;

    /// <summary>
    /// Each time the game advances, this fixed amount of time has passed
    /// </summary>
    constexpr double seconds_per_advance = 1.0 / 60.0;
    constexpr float seconds_per_advance_f = 1.0f / 60.0f;

    constexpr double pi = 3.1415926535897932384626433832795; ///< PI
    constexpr double pi2 = 6.283185307179586476925286766559; ///< 2 * PI
}
