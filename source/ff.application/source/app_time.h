#pragma once

namespace ff
{
    struct app_time_t
    {
        size_t frame_count;
        size_t advance_count;
        size_t render_count;
        double app_seconds;
        double clock_seconds;
        double unused_advance_seconds;
        double time_scale;
    };
}
