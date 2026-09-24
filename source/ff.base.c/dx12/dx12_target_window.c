#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_queue.h"
#include "dx12/dx12_target_window.h"

static const ff_dx12_pacing_stage s_pacing_stages[FF_DX12_PACING_STAGE_COUNT] =
{
    { .latency = 1, .vsync = true },
    { .latency = 1, .vsync = false },
    { .latency = 2, .vsync = true },
    { .latency = 2, .vsync = false },
};

static const double s_seconds_per_update = 1.0 / 60.0;

static int64_t perf_counter(void)
{
    LARGE_INTEGER value;
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

static double perf_frequency(void)
{
    static double frequency;

    if (!frequency)
    {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        frequency = (double)value.QuadPart;
    }

    return frequency;
}

uint32_t ff_dx12_target_window_pacing_latency(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target && target->pacing.stage < FF_DX12_PACING_STAGE_COUNT, 1);
    return s_pacing_stages[target->pacing.stage].latency;
}

bool ff_dx12_target_window_pacing_vsync(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target && target->pacing.stage < FF_DX12_PACING_STAGE_COUNT, true);
    return s_pacing_stages[target->pacing.stage].vsync;
}

DXGI_FORMAT ff_dx12_target_window_format(void)
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

static void reset_pacing(ff_dx12_target_window* target)
{
    target->pacing.average = s_seconds_per_update;
    target->pacing.last_tick = 0;
    target->pacing.count = 0;
    target->pacing.stage = 0;
}

static void close_latency_handle(ff_dx12_target_window* target)
{
    if (target->latency_handle)
    {
        CloseHandle(target->latency_handle);
        target->latency_handle = NULL;
    }
}

// Drops every reference this object holds to the swap chain's buffers. ResizeBuffers fails unless
// all of them are gone, and ff_dx12_resource_destroy only defers the Release onto the keep-alive
// list, so the caller must also drain that list before resizing.
static void destroy_back_buffers(ff_dx12_target_window* target)
{
    for (size_t i = 0; i < target->back_buffers_created; i++)
    {
        ff_dx12_resource_destroy(&target->back_buffers[i]);
    }

    target->back_buffers_created = 0;
}

static bool create_back_buffers(ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(!target->back_buffers_created, false);

    for (size_t i = 0; i < FF_DX12_TARGET_WINDOW_BUFFER_COUNT; i++)
    {
        ID3D12Resource* resource = NULL;
        if (FAILED(IDXGISwapChain4_GetBuffer(target->swap_chain, (UINT)i, &IID_ID3D12Resource, (void**)&resource)))
        {
            destroy_back_buffers(target);
            ff_dx12_device_fatal_error(FF_SVL("Swap chain get buffer failed"));
            return false;
        }

        wchar_t name[64];
        _snwprintf_s(name, _countof(name), _TRUNCATE, L"Swap chain back buffer %zu", i);

        ff_arena_declare_stack(name_arena, 256);
        ff_string_view utf8_name = ff_wide_to_utf8(ff_wz_view(name), &name_arena, true);
        const bool init_ok = ff_dx12_resource_init_external(&target->back_buffers[i], utf8_name, resource);
        ff_arena_destroy(&name_arena);

        // init_external takes its own reference, so this one is always dropped.
        ID3D12Resource_Release(resource);

        if (!init_ok)
        {
            destroy_back_buffers(target);
            return false;
        }

        // Counted as created only once init succeeded, so a partial failure never destroys an
        // entry that was never initialized.
        target->back_buffers_created = i + 1;

        ff_dx12_resource_create_target_view(&target->back_buffers[i],
            ff_dx12_descriptor_range_cpu_handle(&target->views, i), 0, 1, 0);
    }

    return true;
}

static bool apply_latency(ff_dx12_target_window* target)
{
    close_latency_handle(target);

    if (FAILED(IDXGISwapChain4_SetMaximumFrameLatency(target->swap_chain,
        ff_dx12_target_window_pacing_latency(target))))
    {
        ff_dx12_device_fatal_error(FF_SVL("Swap chain failed to set frame latency"));
        return false;
    }

    target->latency_handle = IDXGISwapChain4_GetFrameLatencyWaitableObject(target->swap_chain);
    return true;
}

static bool create_swap_chain(ff_dx12_target_window* target, size_t width, size_t height)
{
    DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
    desc.Width = (UINT)width;
    desc.Height = (UINT)height;
    desc.Format = ff_dx12_target_window_format();
    desc.SampleDesc.Count = 1;
    desc.BufferCount = FF_DX12_TARGET_WINDOW_BUFFER_COUNT;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ID3D12CommandQueue* command_queue = ff_dx12_queue_command_queue(ff_dx12_direct_queue());
    FF_ASSERT_RET_VAL(command_queue, false);

    IDXGISwapChain1* new_swap_chain = NULL;
    if (FAILED(IDXGIFactory6_CreateSwapChainForHwnd(ff_dx12_factory(), (IUnknown*)command_queue,
        target->hwnd, &desc, NULL, NULL, &new_swap_chain)))
    {
        ff_dx12_device_fatal_error(FF_SVL("Swap chain creation failed"));
        return false;
    }

    HRESULT hr = IDXGISwapChain1_QueryInterface(new_swap_chain, &IID_IDXGISwapChain4, (void**)&target->swap_chain);
    IDXGISwapChain1_Release(new_swap_chain);

    if (FAILED(hr))
    {
        target->swap_chain = NULL;
        ff_dx12_device_fatal_error(FF_SVL("Swap chain creation failed"));
        return false;
    }

    // The swap chain, not DXGI, owns window mode changes here: full screen is a borderless window
    // style handled by the window layer, so DXGI must not react to alt-enter or window changes.
    IDXGIFactory6_MakeWindowAssociation(ff_dx12_factory(), target->hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

    return true;
}

bool ff_dx12_target_window_init(ff_dx12_target_window* target, HWND hwnd)
{
    FF_ASSERT_RET_VAL(target && hwnd && IsWindow(hwnd), false);

    *target = (ff_dx12_target_window){ 0 };
    target->hwnd = hwnd;
    reset_pacing(target);

    target->views = ff_dx12_cpu_descriptor_allocator_alloc(ff_dx12_cpu_target_descriptors(),
        FF_DX12_TARGET_WINDOW_BUFFER_COUNT);

    if (!ff_dx12_descriptor_range_valid(&target->views))
    {
        *target = (ff_dx12_target_window){ 0 };
        return false;
    }

    RECT rect = { 0 };
    GetClientRect(hwnd, &rect);

    if (!ff_dx12_target_window_set_size(target, (size_t)(rect.right - rect.left), (size_t)(rect.bottom - rect.top)))
    {
        ff_dx12_target_window_destroy(target);
        return false;
    }

    return true;
}

void ff_dx12_target_window_destroy(ff_dx12_target_window* target)
{
    FF_CHECK_RET(target);

    // The GPU may still be presenting from these buffers, and the swap chain is about to go away,
    // so everything has to be retired before the references are dropped.
    ff_dx12_wait_for_idle();

    destroy_back_buffers(target);
    close_latency_handle(target);
    ff_dx12_descriptor_range_free(&target->views);

    if (target->swap_chain)
    {
        IDXGISwapChain4_Release(target->swap_chain);
        target->swap_chain = NULL;
    }

    *target = (ff_dx12_target_window){ 0 };
}

bool ff_dx12_target_window_valid(const ff_dx12_target_window* target)
{
    return target && target->swap_chain &&
        target->back_buffers_created == FF_DX12_TARGET_WINDOW_BUFFER_COUNT &&
        ff_dx12_device_valid();
}

bool ff_dx12_target_window_set_size(ff_dx12_target_window* target, size_t width, size_t height)
{
    FF_ASSERT_RET_VAL(target && target->hwnd, false);

    // A minimized window has a zero-sized client area, which DXGI rejects.
    width = width ? width : 1;
    height = height ? height : 1;

    // Nothing to do only when the swap chain is fully built at this size already. A previous
    // failure can leave the buffers missing at the right size, which still needs a rebuild.
    const bool size_changed = (width != target->width) || (height != target->height);
    const bool fully_built = target->swap_chain &&
        target->back_buffers_created == FF_DX12_TARGET_WINDOW_BUFFER_COUNT;
    FF_CHECK_RET_VAL(size_changed || !fully_built, true);

    // ResizeBuffers requires every reference to the old buffers to be gone. Destroying them only
    // queues the Release on the keep-alive list, so the list has to be drained too; wait_for_idle
    // does both, and is also what makes it safe for the GPU to stop using them.
    ff_dx12_wait_for_idle();
    destroy_back_buffers(target);
    close_latency_handle(target);

    target->width = width;
    target->height = height;

    if (target->swap_chain)
    {
        DXGI_SWAP_CHAIN_DESC1 desc;
        if (FAILED(IDXGISwapChain4_GetDesc1(target->swap_chain, &desc)) ||
            FAILED(IDXGISwapChain4_ResizeBuffers(target->swap_chain, FF_DX12_TARGET_WINDOW_BUFFER_COUNT,
                (UINT)width, (UINT)height, desc.Format, desc.Flags)))
        {
            ff_dx12_device_fatal_error(FF_SVL("Swap chain resize failed"));
            return false;
        }
    }
    else if (!create_swap_chain(target, width, height))
    {
        return false;
    }

    // Resizing invalidates the pacing history, since frame times across a resize say nothing about
    // how the new size performs.
    reset_pacing(target);

    return apply_latency(target) && create_back_buffers(target);
}

size_t ff_dx12_target_window_width(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, 0);
    return target->width;
}

size_t ff_dx12_target_window_height(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, 0);
    return target->height;
}

size_t ff_dx12_target_window_buffer_count(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target && target->swap_chain, 0);

    DXGI_SWAP_CHAIN_DESC desc;
    FF_CHECK_RET_VAL(SUCCEEDED(IDXGISwapChain4_GetDesc(target->swap_chain, &desc)), 0);

    return (size_t)desc.BufferCount;
}

ff_dx12_resource* ff_dx12_target_window_resource(ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(ff_dx12_target_window_valid(target), NULL);

    const UINT index = IDXGISwapChain4_GetCurrentBackBufferIndex(target->swap_chain);
    FF_ASSERT_RET_VAL(index < FF_DX12_TARGET_WINDOW_BUFFER_COUNT, NULL);

    return &target->back_buffers[index];
}

D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_target_window_view(ff_dx12_target_window* target)
{
    const D3D12_CPU_DESCRIPTOR_HANDLE empty = { 0 };
    FF_ASSERT_RET_VAL(ff_dx12_target_window_valid(target), empty);

    const UINT index = IDXGISwapChain4_GetCurrentBackBufferIndex(target->swap_chain);
    FF_ASSERT_RET_VAL(index < FF_DX12_TARGET_WINDOW_BUFFER_COUNT, empty);

    return ff_dx12_descriptor_range_cpu_handle(&target->views, index);
}

ff_dx12_target_range ff_dx12_target_window_range(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, ((ff_dx12_target_range) { 0 }));

    ff_dx12_target_range range =
    {
        .array_start = 0,
        .array_size = 1,
        .mip_start = 0,
        .mip_size = 1,
    };

    return range;
}

void ff_dx12_target_window_clear(ff_dx12_target_window* target, ff_dx12_commands* commands, const float color[4])
{
    FF_CHECK_RET(ff_dx12_target_window_valid(target) && commands && color);

    ff_dx12_commands_clear_target(commands, ff_dx12_target_window_resource(target),
        ff_dx12_target_window_view(target), color);
}

void ff_dx12_target_window_discard(ff_dx12_target_window* target, ff_dx12_commands* commands)
{
    FF_CHECK_RET(ff_dx12_target_window_valid(target) && commands);
    ff_dx12_commands_discard_target(commands, ff_dx12_target_window_resource(target));
}

bool ff_dx12_target_window_begin_render(ff_dx12_target_window* target, ff_dx12_commands* commands, const float* clear_color)
{
    FF_CHECK_RET_VAL(ff_dx12_target_window_valid(target) && commands, false);

    if (clear_color)
    {
        FF_ASSERT_MSG(!clear_color[0] && !clear_color[1] && !clear_color[2] && clear_color[3] == 1.0f,
            "Swap chain render targets must be cleared to black");
        ff_dx12_target_window_clear(target, commands, clear_color);
    }
    else
    {
        ff_dx12_target_window_discard(target, commands);
    }

    return true;
}

static bool update_pacing(ff_dx12_target_window* target)
{
    const size_t ema_window = 16;
    const double ema_alpha = 1.0 / (double)ema_window;
    const double good_fps = 58.0;
    const double bad_fps = 54.0;
    const size_t window_count_to_improve = 2;
    const size_t window_count_ignore_after_resize = 1;

    const int64_t now = perf_counter();
    const int64_t last = target->pacing.last_tick;
    target->pacing.last_tick = now;

    // The first frame after init or a resize has no previous tick to measure against.
    FF_CHECK_RET_VAL(last, true);

    const double frame_time = (double)(now - last) / perf_frequency();
    target->pacing.average = target->pacing.average * (1.0 - ema_alpha) + frame_time * ema_alpha;

    // Only reconsider the stage once per window of frames, so one slow frame can't move it.
    FF_CHECK_RET_VAL(!(++target->pacing.count % ema_window), true);

    const size_t window_count = target->pacing.count / ema_window;
    const uint32_t before_latency = ff_dx12_target_window_pacing_latency(target);

    if (target->pacing.average <= (1.0 / good_fps) && target->pacing.stage > 0 &&
        window_count >= window_count_to_improve)
    {
        target->pacing.stage--;
    }
    else if (target->pacing.average >= (1.0 / bad_fps) &&
        (target->pacing.stage > 0 || window_count > window_count_ignore_after_resize))
    {
        if (target->pacing.stage + 1 < FF_DX12_PACING_STAGE_COUNT)
        {
            target->pacing.stage++;
        }
    }

    if (before_latency != ff_dx12_target_window_pacing_latency(target))
    {
        return apply_latency(target);
    }

    return true;
}

bool ff_dx12_target_window_end_render(ff_dx12_target_window* target, ff_dx12_commands* commands)
{
    FF_CHECK_RET_VAL(ff_dx12_target_window_valid(target) && commands, false);

    // Pacing the CPU against the frame latency handle is the one place a wait on the GPU belongs.
    // It happens before this frame's work is submitted, so it never stalls in the middle of one.
    if (target->latency_handle)
    {
        WaitForSingleObjectEx(target->latency_handle, INFINITE, FALSE);
    }

    ff_dx12_commands_resource_state(commands, ff_dx12_target_window_resource(target),
        D3D12_RESOURCE_STATE_PRESENT, 0, 1, 0, 1);
    ff_dx12_queue_execute(ff_dx12_commands_queue(commands), commands);

    const HRESULT hr = IDXGISwapChain4_Present(target->swap_chain,
        ff_dx12_target_window_pacing_vsync(target) ? 1 : 0, 0);

    const bool paced = update_pacing(target);

    return paced && ff_dx12_device_valid() &&
        hr != DXGI_ERROR_DEVICE_RESET && hr != DXGI_ERROR_DEVICE_REMOVED;
}
