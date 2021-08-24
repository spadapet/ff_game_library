#pragma once

namespace ff
{
#if DXVER == 11
    class dx11_device_state;
    class dx11_object_cache;
#elif DXVER == 12
    class dx12_command_queue;
    class dx12_command_queues;
    class dx12_commands;
    class dx12_cpu_descriptor_allocator;
    class dx12_gpu_descriptor_allocator;
#endif
    class target_base;
    class target_window_base;
}

namespace ff::internal
{
    class graphics_child_base;
}

namespace ff::graphics
{
    bool reset(bool force);

    IDXGIFactoryX* dxgi_factory();
    IDWriteFactoryX* write_factory();
    IDWriteInMemoryFontFileLoader* write_font_loader();

    IDXGIFactoryX* dxgi_factory_for_device();
    IDXGIAdapterX* dxgi_adapter_for_device();
    D3D_FEATURE_LEVEL dx_feature_level();

#if DXVER == 11
    IDXGIDeviceX* dxgi_device();
    ID3D11DeviceX* dx11_device();
    ID3D11DeviceContextX* dx11_device_context();
    ff::dx11_device_state& dx11_device_state();
    ff::dx11_object_cache& dx11_object_cache();
#elif DXVER == 12
    ID3D12DeviceX* dx12_device();

    DXGI_QUERY_VIDEO_MEMORY_INFO dx12_memory_info();
    ff::signal_sink<const DXGI_QUERY_VIDEO_MEMORY_INFO&>& dx12_memory_info_changed_sink();
    void dx12_reserve_memory(size_t size);
    void dx12_unreserve_memory(size_t size);

    ff::dx12_commands& dx12_direct_commands();
    ff::dx12_command_queues& dx12_queues();
    ff::dx12_command_queue& dx12_direct_queue();
    ff::dx12_command_queue& dx12_copy_queue();
    ff::dx12_command_queue& dx12_compute_queue();

    ff::dx12_cpu_descriptor_allocator& dx12_descriptors_buffer();
    ff::dx12_cpu_descriptor_allocator& dx12_descriptors_sampler();
    ff::dx12_cpu_descriptor_allocator& dx12_descriptors_target();
    ff::dx12_cpu_descriptor_allocator& dx12_descriptors_depth();
    ff::dx12_gpu_descriptor_allocator& dx12_descriptors_gpu_buffer();
    ff::dx12_gpu_descriptor_allocator& dx12_descriptors_gpu_sampler();
#endif
}

namespace ff::graphics::defer
{
    void set_full_screen_target(ff::target_window_base* target);
    void remove_target(ff::target_window_base* target);
    void resize_target(ff::target_window_base* target, const ff::window_size& size);
    void validate_device(bool force);
    void full_screen(bool value);
}

namespace ff::internal::graphics
{
    bool init();
    void destroy();

    bool init_d3d(bool for_reset);
    void destroy_d3d(bool for_reset);
    bool d3d_device_disconnected();
    void render_frame_complete(ff::target_base* target, uint64_t fence_value);
    ff::signal_sink<uint64_t>& render_frame_complete_sink();

    void add_child(ff::internal::graphics_child_base* child);
    void remove_child(ff::internal::graphics_child_base* child);
}
