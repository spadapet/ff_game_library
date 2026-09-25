#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_queue.h"
#include "dx12/dx12_target_window.h"

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
    FF_ASSERT_RET_VAL(target, 1);
    return internal_ff_dx12_pacing_latency(&target->pacing);
}

bool ff_dx12_target_window_pacing_vsync(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, true);
    return internal_ff_dx12_pacing_vsync(&target->pacing);
}

double ff_dx12_target_window_pacing_average_seconds(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, 0.0);
    return target->pacing.average_seconds;
}

uint64_t ff_dx12_target_window_pacing_late_frames(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, 0);
    return target->pacing.total_late_frames;
}

size_t ff_dx12_target_window_pacing_stage(const ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target, 0);
    return target->pacing.stage;
}

DXGI_FORMAT ff_dx12_target_window_format(void)
{
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}

static void reset_pacing(ff_dx12_target_window* target)
{
    internal_ff_dx12_pacing_init(&target->pacing, internal_ff_dx12_pacing_refresh_seconds(target->hwnd));
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

    ff_dx12_add_device_child(&target->device_child, target, ff_dx12_device_child_type_target_window);

    return true;
}

void ff_dx12_target_window_destroy(ff_dx12_target_window* target)
{
    FF_CHECK_RET(target);

    // Must happen before the object dies, or the deferred queue would hold a pointer to it.
    ff_dx12_cancel_deferred_target(target);

    ff_dx12_remove_device_child(&target->device_child);

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

    // Tears down back buffers and waits for idle, so the window thread must go through
    // ff_dx12_defer_resize_target instead of calling this directly.
    FF_DX12_ASSERT_OWNER();

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

    // Resizing invalidates the in-flight measurements, since frame times spanning a resize say
    // nothing about how the new size performs. The learned stage is kept: a machine that could not
    // hold stage 0 a moment ago is unlikely to manage it now that it has more pixels to fill.
    // The monitor may also have changed, so re-read its refresh rate.
    target->pacing.refresh_seconds = internal_ff_dx12_pacing_refresh_seconds(target->hwnd);
    internal_ff_dx12_pacing_interrupt(&target->pacing);

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
    const int64_t now = perf_counter();
    const int64_t last = target->pacing.last_tick;
    target->pacing.last_tick = now;

    // The first frame after init, a resize or a reset has no previous tick to measure against.
    FF_CHECK_RET_VAL(last, true);

    double frame_seconds = (double)(now - last) / perf_frequency();

    // After a missed vblank the latency handle is already signaled, so the following frame does
    // not block and is measured back-to-back with this one. That is the tail of one long frame,
    // not a genuinely fast frame, and feeding it in as-is is what made the old ladder see
    // above-refresh frame rates and oscillate. Charge it at the refresh interval instead.
    if (frame_seconds < target->pacing.refresh_seconds)
    {
        frame_seconds = target->pacing.refresh_seconds;
    }

    FF_CHECK_RET_VAL(internal_ff_dx12_pacing_add_frame(&target->pacing, frame_seconds), true);

    return apply_latency(target);
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

void internal_ff_dx12_target_window_before_reset(ff_dx12_target_window* target)
{
    FF_CHECK_RET(target);

    // No wait_for_idle here. The device is being torn down precisely because it can no longer
    // make progress, so waiting on its queues could block forever. Dropping the references is
    // safe for the same reason: the device dying is what retires the work.
    destroy_back_buffers(target);
    close_latency_handle(target);
    reset_pacing(target);

    if (target->swap_chain)
    {
        IDXGISwapChain4_Release(target->swap_chain);
        target->swap_chain = NULL;
    }
}

bool internal_ff_dx12_target_window_reset(ff_dx12_target_window* target)
{
    FF_ASSERT_RET_VAL(target && target->hwnd, false);
    FF_ASSERT_RET_VAL(!target->swap_chain, false);

    // The descriptor range survives the reset, but a destroyed window means there is nothing to
    // present to and no size to query.
    FF_CHECK_RET_VAL(IsWindow(target->hwnd), false);

    RECT rect = { 0 };
    GetClientRect(target->hwnd, &rect);

    // Forces a full rebuild rather than the early-out in set_size, since the swap chain is gone
    // even though the recorded size may be unchanged.
    target->width = 0;
    target->height = 0;

    return ff_dx12_target_window_set_size(target,
        (size_t)(rect.right - rect.left), (size_t)(rect.bottom - rect.top));
}