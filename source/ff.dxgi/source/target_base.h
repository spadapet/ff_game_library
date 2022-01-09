#pragma once

namespace ff::dxgi
{
    class command_context_base;
    class target_access_base;

    class target_base
    {
    public:
        virtual ~target_base() = default;

        virtual void clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color) = 0;
        virtual bool begin_render(const DirectX::XMFLOAT4* clear_color = nullptr) = 0;
        virtual bool end_render() = 0;

        virtual ff::dxgi::target_access_base& target_access() = 0;
        virtual size_t target_array_start() const = 0;
        virtual size_t target_array_size() const = 0;
        virtual size_t target_mip_start() const = 0;
        virtual size_t target_mip_size() const = 0;
        virtual DXGI_FORMAT format() const = 0;
        virtual ff::window_size size() const = 0;
    };
}
