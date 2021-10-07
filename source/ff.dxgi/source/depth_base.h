#pragma once

namespace ff::dxgi
{
    class command_context_base;

    class depth_base
    {
    public:
        virtual ~depth_base() = default;

        virtual ff::point_int size() const = 0;
        virtual bool size(const ff::point_int& size) = 0;
        virtual size_t sample_count() const = 0;
        virtual void clear(ff::dxgi::command_context_base& context, float depth, uint8_t stencil) const = 0;
        virtual void clear_depth(ff::dxgi::command_context_base& context, float depth) const = 0;
        virtual void clear_stencil(ff::dxgi::command_context_base& context, uint8_t stencil) const = 0;
    };
}
