#pragma once

namespace ff
{
    struct graphics_counters
    {
        size_t draw;
        size_t clear;
        size_t depth_clear;
        size_t map; // CPU to GPU (indirect)
        size_t update; // CPU to GPU (direct)
        size_t copy; // GPU copy
    };
}
