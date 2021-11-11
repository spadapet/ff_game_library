#pragma once

namespace ff::dx12
{
    class commands;
    class cpu_descriptor_allocator;
    class fence_values;
    class gpu_descriptor_allocator;
    class mem_allocator;
    class mem_allocator_ring;
    class queue;
    class resource;
    enum class device_reset_priority;

    bool reset(bool force);
    void trim();

    D3D_FEATURE_LEVEL feature_level();
    IDXGIFactoryX* factory();
    IDXGIAdapterX* adapter();
    ID3D12DeviceX* device();

    size_t frame_count();
    void frame_complete();
    void wait_for_idle();
    ff::signal_sink<size_t>& frame_complete_sink();
    void keep_alive_resource(ff::dx12::resource&& resource, ff::dx12::fence_values&& fence_values);

    ff::dx12::queue& direct_queue();
    ff::dx12::queue& copy_queue();
    ff::dx12::queue& compute_queue();

    ff::dx12::cpu_descriptor_allocator& cpu_buffer_descriptors();
    ff::dx12::cpu_descriptor_allocator& cpu_sampler_descriptors();
    ff::dx12::cpu_descriptor_allocator& cpu_target_descriptors();
    ff::dx12::cpu_descriptor_allocator& cpu_depth_descriptors();
    ff::dx12::gpu_descriptor_allocator& gpu_view_descriptors();
    ff::dx12::gpu_descriptor_allocator& gpu_sampler_descriptors();

    ff::dx12::mem_allocator_ring& upload_allocator();
    ff::dx12::mem_allocator_ring& readback_allocator();
    ff::dx12::mem_allocator_ring& dynamic_buffer_allocator();
    ff::dx12::mem_allocator& static_buffer_allocator();
    ff::dx12::mem_allocator& texture_allocator();

    bool init_globals(D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0);
    void destroy_globals();

    void add_device_child(ff::dxgi::device_child_base* child, ff::dx12::device_reset_priority reset_priority);
    void remove_device_child(ff::dxgi::device_child_base* child);
}
