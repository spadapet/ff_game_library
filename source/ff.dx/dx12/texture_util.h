#pragma once

namespace ff::dx12
{
    class resource;

    size_t fix_sample_count(DXGI_FORMAT format, size_t sample_count);
    void create_shader_view(const ff::dx12::resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
    void create_target_view(const ff::dx12::resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE view, size_t array_start = 0, size_t array_count = 1, size_t mip_level = 0);
}
