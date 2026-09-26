#pragma once

#include "../base/string.h"

// Named, colored scopes around GPU work, shown as a nested timeline by PIX and RenderDoc. Ends
// have to match begins on the same command list, since the debugger builds the tree from that
// nesting.
typedef enum ff_dx12_gpu_event
{
    ff_dx12_gpu_event_none,
    ff_dx12_gpu_event_render_frame,
    ff_dx12_gpu_event_draw_2d,
    ff_dx12_gpu_event_draw_batch,
    ff_dx12_gpu_event_draw_imgui,
    ff_dx12_gpu_event_update_palette,
    ff_dx12_gpu_event_clear_target,
    ff_dx12_gpu_event_copy_resource,
    ff_dx12_gpu_event_update_texture,
    ff_dx12_gpu_event_count,
} ff_dx12_gpu_event;

ff_wstring_view ff_dx12_gpu_event_name(ff_dx12_gpu_event type);
