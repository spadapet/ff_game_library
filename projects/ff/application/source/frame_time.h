#pragma once

namespace ff
{
    enum class advance_type
    {
        running,
        single_step,
        stopped,
    };

    struct frame_time_t
    {
        static const size_t max_advance_count = 4;
        static const size_t max_advance_multiplier = 4;

        ff::graphics_counters graphics_counters;
        size_t advance_count;
        std::array<int64_t, ff::frame_time_t::max_advance_count * ff::frame_time_t::max_advance_multiplier> advance_times;
        int64_t render_time;
        int64_t flip_time;
    };
}
