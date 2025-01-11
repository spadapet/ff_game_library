#pragma once

#include "../dxgi/texture_metadata_base.h"
#include "../dxgi/texture_view_base.h"

namespace ff::dxgi
{
    class command_context_base;
    enum class sprite_type;

    class texture_base : public ff::dxgi::texture_metadata_base, public ff::dxgi::texture_view_base
    {
    public:
        virtual ~texture_base() = default;

        virtual ff::dxgi::sprite_type sprite_type() const = 0;
        virtual std::shared_ptr<DirectX::ScratchImage> data() const = 0;
        virtual bool update(ff::dxgi::command_context_base& context, size_t array_index, size_t mip_index, const ff::point_size& pos, const DirectX::Image& data) = 0;
    };
}
