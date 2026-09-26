#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_gpu_event.h"

static const ff_wstring_view s_event_names[ff_dx12_gpu_event_count] =
{
    FF_WSVL_INIT(L""),
    FF_WSVL_INIT(L"render_frame"),
    FF_WSVL_INIT(L"draw_2d"),
    FF_WSVL_INIT(L"draw_batch"),
    FF_WSVL_INIT(L"draw_imgui"),
    FF_WSVL_INIT(L"update_palette"),
    FF_WSVL_INIT(L"clear_target"),
    FF_WSVL_INIT(L"copy_resource"),
    FF_WSVL_INIT(L"update_texture"),
};

static_assert(sizeof(s_event_names) / sizeof(s_event_names[0]) == ff_dx12_gpu_event_count,
    "gpu event name table must cover every ff_dx12_gpu_event");

ff_wstring_view ff_dx12_gpu_event_name(ff_dx12_gpu_event type)
{
    FF_ASSERT_RET_VAL(type >= 0 && type < ff_dx12_gpu_event_count, ff_wstring_view_empty());
    return s_event_names[type];
}
