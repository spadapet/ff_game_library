#pragma once

namespace ff::dxgi
{
    class command_context_base;
    class target_access_base;

    class target_base
    {
    public:
        virtual ~target_base() = default;

        virtual bool pre_render(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4* clear_color = nullptr) = 0;
        virtual bool post_render(ff::dxgi::command_context_base& context) = 0;
        virtual ff::signal_sink<target_base*>& render_presented() = 0;

        virtual ff::dxgi::target_access_base& target_access() = 0;
        virtual DXGI_FORMAT format() const = 0;
        virtual ff::window_size size() const = 0;
    };
}
