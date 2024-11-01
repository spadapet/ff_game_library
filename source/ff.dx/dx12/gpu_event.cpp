#include "pch.h"
#include "dx12/gpu_event.h"

#ifdef _WIN64
#include <pix3.h>
#endif

const char* ff::dx12::gpu_event_name(ff::dx12::gpu_event type)
{
    switch (type)
    {
        case ff::dx12::gpu_event::draw_2d: return "draw_2d";
        case ff::dx12::gpu_event::draw_batch: return "draw_batch";
        case ff::dx12::gpu_event::render_frame: return "render_frame";
        case ff::dx12::gpu_event::update_palette: return "update_palette";
        case ff::dx12::gpu_event::noesis_offscreen: return "noesis_offscreen";
        case ff::dx12::gpu_event::noesis_view: return "noesis_view";
        default: debug_fail_ret_val("");
    }
}

uint64_t ff::dx12::gpu_event_color(ff::dx12::gpu_event type)
{
#ifdef _WIN64
    return ::PIX_COLOR_INDEX(static_cast<uint8_t>(type));
#else
    return 0;
#endif
}
