#pragma once

#include "dx12_commands.h"
#include "dx12_descriptor_range.h"
#include "dx12_device_child.h"
#include "dx12_resource.h"

#define FF_DX12_TARGET_WINDOW_BUFFER_COUNT 2

// Frame pacing ladder. Each stage trades latency and vsync for a more forgiving frame budget, so
// stage 0 is the best-looking and the last stage is the most tolerant of a slow GPU.
typedef struct ff_dx12_pacing_stage
{
    uint32_t latency;
    bool vsync;
} ff_dx12_pacing_stage;

#define FF_DX12_PACING_STAGE_COUNT 4

// A swap chain and its back buffers, presented to one window. The back buffers are external
// resources: the swap chain owns the memory, and this only holds references plus the render target
// views. Everything here runs on the thread that renders.
typedef struct ff_dx12_target_window
{
    HWND hwnd;
    IDXGISwapChain4* swap_chain;
    HANDLE latency_handle;
    ff_dx12_device_child device_child;

    ff_dx12_resource back_buffers[FF_DX12_TARGET_WINDOW_BUFFER_COUNT];

    // How many entries of back_buffers are initialized. Only ever the full count or 0 in practice,
    // but tracked as a count so a partial failure destroys exactly what was built.
    size_t back_buffers_created;
    ff_dx12_descriptor_range views;

    size_t width;
    size_t height;

    struct
    {
        double average;
        int64_t last_tick;
        size_t count;
        size_t stage;
    } pacing;
} ff_dx12_target_window;

bool ff_dx12_target_window_init(ff_dx12_target_window* target, HWND hwnd);
void ff_dx12_target_window_destroy(ff_dx12_target_window* target);

bool ff_dx12_target_window_valid(const ff_dx12_target_window* target);

// Resizes the swap chain buffers. A width or height of 0 is clamped to 1, since a minimized window
// reports a zero client area and DXGI rejects zero-sized buffers.
bool ff_dx12_target_window_set_size(ff_dx12_target_window* target, size_t width, size_t height);

size_t ff_dx12_target_window_width(const ff_dx12_target_window* target);
size_t ff_dx12_target_window_height(const ff_dx12_target_window* target);
DXGI_FORMAT ff_dx12_target_window_format(void);
size_t ff_dx12_target_window_buffer_count(const ff_dx12_target_window* target);

// The back buffer that the next frame renders into, which is whichever one the swap chain reports
// as current. Both change after every present.
ff_dx12_resource* ff_dx12_target_window_resource(ff_dx12_target_window* target);
D3D12_CPU_DESCRIPTOR_HANDLE ff_dx12_target_window_view(ff_dx12_target_window* target);
ff_dx12_target_range ff_dx12_target_window_range(const ff_dx12_target_window* target);

void ff_dx12_target_window_clear(ff_dx12_target_window* target, ff_dx12_commands* commands, const float color[4]);
void ff_dx12_target_window_discard(ff_dx12_target_window* target, ff_dx12_commands* commands);

// clear_color of NULL discards the previous contents instead of clearing them. A swap chain back
// buffer can only be cleared to black.
bool ff_dx12_target_window_begin_render(ff_dx12_target_window* target, ff_dx12_commands* commands, const float* clear_color);

// Waits for the frame latency handle, transitions the back buffer to PRESENT, executes 'commands',
// and presents. The latency wait is the one CPU/GPU sync that belongs in a frame, and it happens
// before the work is submitted so it paces the CPU rather than stalling mid-frame. Returns false
// when the device was reset or removed, which the caller handles by rebuilding the device.
bool ff_dx12_target_window_end_render(ff_dx12_target_window* target, ff_dx12_commands* commands);

uint32_t ff_dx12_target_window_pacing_latency(const ff_dx12_target_window* target);
bool ff_dx12_target_window_pacing_vsync(const ff_dx12_target_window* target);

// Device reset. before_reset drops the back buffers, the latency handle and the swap chain
// itself: a swap chain is bound to the command queue it was created with, so it cannot outlive
// the device. reset rebuilds it at the window's current client size.
void internal_ff_dx12_target_window_before_reset(ff_dx12_target_window* target);
bool internal_ff_dx12_target_window_reset(ff_dx12_target_window* target);
