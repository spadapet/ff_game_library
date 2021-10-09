#pragma once

namespace ff::dx12
{
    class resource;

    class target_access : public ff::dxgi::target_access_base
    {
    public:
        static target_access& get(ff::dxgi::target_base& obj);

        virtual ff::dx12::resource& target_texture() = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE target_view() = 0;
    };
}
