#pragma once

/// <summary>
/// Useful global constants
/// </summary>
namespace ff::constants
{
    constexpr bool debug_build = static_cast<bool>(DEBUG);
    constexpr bool profile_build = static_cast<bool>(PROFILE_APP);
    constexpr size_t bits_build = sizeof(size_t) * 8;
    constexpr size_t invalid_size = static_cast<size_t>(-1);
    constexpr DWORD invalid_dword = static_cast<DWORD>(-1);
    constexpr uintmax_t invalid_uintmax = static_cast<std::uintmax_t>(-1);

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
}
