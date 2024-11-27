#pragma once

namespace ff::dxgi
{
    class command_context_base;
    class device_child_base;
    class draw_device_base;
}

namespace ff::dx12
{
    class commands;
    class cpu_descriptor_allocator;
    class fence;
    class fence_values;
    class gpu_descriptor_allocator;
    class mem_allocator;
    class mem_allocator_ring;
    class object_cache;
    class queue;
    class resource;
    enum class device_reset_priority;

    bool reset_device(bool force);
    void trim_device();
    bool device_valid();
    void device_fatal_error(std::string_view reason);
    void wait_for_idle();

    D3D_FEATURE_LEVEL feature_level();
    IDXGIFactory4* factory();
    IDXGIAdapter3* adapter();
    ID3D12Device1* device();
    ff::dx12::object_cache& get_object_cache();
    ff::dxgi::draw_device_base& get_draw_device();

    const DXGI_QUERY_VIDEO_MEMORY_INFO& get_video_memory_info();
    ff::dx12::fence& residency_fence();
    bool supports_create_heap_not_resident();
    void keep_alive_resource(ff::dx12::resource&& resource, ff::dx12::fence_values&& fence_values);
    size_t fix_sample_count(DXGI_FORMAT format, size_t sample_count);

    size_t frame_count();
    ff::dxgi::command_context_base& frame_started();
    void frame_complete();
    ff::dx12::commands& frame_commands();
    ff::signal_sink<size_t>& frame_complete_sink();

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
    ff::dx12::mem_allocator& target_allocator();

    void add_device_child(ff::dxgi::device_child_base* child, ff::dx12::device_reset_priority reset_priority);
    void remove_device_child(ff::dxgi::device_child_base* child);
}

namespace ff::internal::dx12
{
    bool init(D3D_FEATURE_LEVEL feature_level);
    void destroy();

    ff::resource_object_provider& shader_resources();
}
