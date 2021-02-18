#pragma once

/// <summary>
/// Useful global constants
/// </summary>
namespace ff::constants
{
    const bool debug_build = static_cast<bool>(DEBUG);
    const size_t invalid_size = static_cast<size_t>(-1);

    /// <summary>
    /// Fixed amount of times per second that the game loop advances
    /// </summary>
    const double advances_per_second = 60.0;
    const float advances_per_second_f = 60.0f;
    const size_t advances_per_second_s = 60;

    /// <summary>
    /// Each time the game advances, this fixed amount of time has passed
    /// </summary>
    const double seconds_per_advance = 1.0 / 60.0;
    const float seconds_per_advance_f = 1.0f / 60.0f;

    const double pi = 3.1415926535897932384626433832795; ///< PI
    const double pi2 = 6.283185307179586476925286766559; ///< 2 * PI
}
