#pragma once

typedef enum ff_dx12_device_child_type
{
    ff_dx12_device_child_type_mem_allocator,
    ff_dx12_device_child_type_cpu_descriptor_allocator,
    ff_dx12_device_child_type_gpu_descriptor_allocator,
    ff_dx12_device_child_type_resource,
    ff_dx12_device_child_type_buffer,
    ff_dx12_device_child_type_texture,
    ff_dx12_device_child_type_texture_view,
    ff_dx12_device_child_type_depth,
    ff_dx12_device_child_type_target_texture,
    ff_dx12_device_child_type_target_window,
    ff_dx12_device_child_type_object_cache,
    ff_dx12_device_child_type_draw_state,
    ff_dx12_device_child_type_palette_data,
    ff_dx12_device_child_type_draw_device,
    ff_dx12_device_child_type_queue,
    ff_dx12_device_child_type_count,
} ff_dx12_device_child_type;

typedef struct ff_dx12_device_child
{
    struct ff_dx12_device_child* prev;
    struct ff_dx12_device_child* next;
    void* owner;
    ff_dx12_device_child_type type;
    bool pending_reset;
    bool registered;
} ff_dx12_device_child;
