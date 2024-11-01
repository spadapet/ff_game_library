#pragma once

#include "../dxgi/texture_view_access_base.h"

namespace ff::dxgi
{
    class texture_view_base;
}

namespace ff::dx12
{
    class texture_view_access : public ff::dxgi::texture_view_access_base
    {
    public:
        static texture_view_access& get(ff::dxgi::texture_view_base& obj);

        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_texture_view() const = 0;
    };
}
