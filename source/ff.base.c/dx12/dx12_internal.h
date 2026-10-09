#pragma once

#include "dx12_device_child.h"
#include "dx12_pacing.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dx12_buffer ff_dx12_buffer;
typedef struct ff_dx12_commands ff_dx12_commands;
typedef struct ff_dx12_cpu_descriptor_allocator ff_dx12_cpu_descriptor_allocator;
typedef struct ff_dx12_depth ff_dx12_depth;
typedef struct ff_dx12_draw_device ff_dx12_draw_device;
typedef struct ff_dx12_draw_state ff_dx12_draw_state;
typedef struct ff_dx12_fence_values ff_dx12_fence_values;
typedef struct ff_dx12_gpu_descriptor_allocator ff_dx12_gpu_descriptor_allocator;
typedef struct ff_dx12_heap ff_dx12_heap;
typedef struct ff_dx12_mem_allocator ff_dx12_mem_allocator;
typedef struct ff_dx12_object_cache ff_dx12_object_cache;
typedef struct ff_dx12_palette_data ff_dx12_palette_data;
typedef struct ff_dx12_queue ff_dx12_queue;
typedef struct ff_dx12_resource ff_dx12_resource;
typedef struct ff_dx12_target_texture ff_dx12_target_texture;
typedef struct ff_dx12_target_window ff_dx12_target_window;
typedef struct ff_dx12_texture ff_dx12_texture;
typedef struct ff_dx12_texture_view ff_dx12_texture_view;

bool internal_ff_dx12_fence_values_copy(ff_dx12_fence_values* destination, const ff_dx12_fence_values* source, ff_arena* arena);
void internal_ff_dx12_forget_resource(ff_dx12_resource* resource);
size_t internal_ff_dx12_keep_alive_arena_buffer_count(void);

void internal_ff_dx12_pacing_init(ff_dx12_pacing* pacing, double refresh_seconds, ff_dx12_pacing_mode mode);
void internal_ff_dx12_pacing_interrupt(ff_dx12_pacing* pacing);
bool internal_ff_dx12_pacing_add_frame(ff_dx12_pacing* pacing, double frame_seconds);
bool internal_ff_dx12_pacing_add_frame_busy(ff_dx12_pacing* pacing, double frame_seconds, double busy_seconds);
uint32_t internal_ff_dx12_pacing_latency(const ff_dx12_pacing* pacing);
bool internal_ff_dx12_pacing_vsync(const ff_dx12_pacing* pacing);
double internal_ff_dx12_pacing_refresh_seconds(HWND hwnd);

bool internal_ff_dx12_init_dxgi(bool for_reset);
void internal_ff_dx12_destroy_dxgi(void);
bool internal_ff_dx12_init_d3d(bool for_reset);
void internal_ff_dx12_destroy_d3d(bool for_reset);
void internal_ff_dx12_clear_fatal_error(void);
void internal_ff_dx12_reset_shutdown(void);

// Internal reset functions

void internal_ff_dx12_device_child_reset_begin(void);
void internal_ff_dx12_device_child_reset_end(void);
void internal_ff_dx12_device_child_walk_begin(ff_dx12_device_child_type type);
void internal_ff_dx12_device_child_walk_end(void);
ff_dx12_device_child* internal_ff_dx12_device_child_walk_next(void);

void internal_ff_dx12_mem_allocator_before_reset(ff_dx12_mem_allocator* allocator);
bool internal_ff_dx12_mem_allocator_reset(ff_dx12_mem_allocator* allocator);

void internal_ff_dx12_cpu_descriptor_allocator_before_reset(ff_dx12_cpu_descriptor_allocator* allocator);
bool internal_ff_dx12_cpu_descriptor_allocator_reset(ff_dx12_cpu_descriptor_allocator* allocator);

void internal_ff_dx12_gpu_descriptor_allocator_before_reset(ff_dx12_gpu_descriptor_allocator* allocator);
bool internal_ff_dx12_gpu_descriptor_allocator_reset(ff_dx12_gpu_descriptor_allocator* allocator);

void internal_ff_dx12_resource_before_reset(ff_dx12_resource* resource);
bool internal_ff_dx12_resource_reset(ff_dx12_resource* resource);

void internal_ff_dx12_buffer_before_reset(ff_dx12_buffer* buffer);
bool internal_ff_dx12_buffer_reset(ff_dx12_buffer* buffer, ff_dx12_commands* commands);

bool internal_ff_dx12_texture_reset(ff_dx12_texture* texture);
bool internal_ff_dx12_texture_view_reset(ff_dx12_texture_view* view);

bool internal_ff_dx12_depth_reset(ff_dx12_depth* depth);

bool internal_ff_dx12_target_texture_reset(ff_dx12_target_texture* target);
void internal_ff_dx12_target_window_before_reset(ff_dx12_target_window* target);
bool internal_ff_dx12_target_window_reset(ff_dx12_target_window* target);

void internal_ff_dx12_object_cache_before_reset(ff_dx12_object_cache* cache);

void internal_ff_dx12_draw_device_before_reset(ff_dx12_draw_device* device);
bool internal_ff_dx12_draw_device_reset(ff_dx12_draw_device* device);

void internal_ff_dx12_draw_state_before_reset(ff_dx12_draw_state* state);
bool internal_ff_dx12_draw_state_reset(ff_dx12_draw_state* state);

bool internal_ff_dx12_palette_data_reset(ff_dx12_palette_data* data, ff_dx12_commands* commands);

void internal_ff_dx12_heap_before_reset(ff_dx12_heap* heap);
bool internal_ff_dx12_heap_reset(ff_dx12_heap* heap);

void internal_ff_dx12_queue_before_reset(ff_dx12_queue* queue);
bool internal_ff_dx12_queue_reset(ff_dx12_queue* queue);
