#pragma once

namespace ff::dx12
{
    enum class gpu_event
    {
        render_frame,
        draw_2d,
        draw_batch,
        draw_imgui,
        update_palette,
    };

    const char* gpu_event_name(ff::dx12::gpu_event type);
    uint64_t gpu_event_color(ff::dx12::gpu_event type);
}
