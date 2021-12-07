#pragma once

#include "descriptor_range.h"
#include "texture_view_access.h"

namespace ff::dx12
{
    class resource;

    class texture : public ff::dxgi::texture_base, public ff::dx12::texture_view_access, private ff::dxgi::device_child_base
    {
    public:
        texture(ff::point_size size, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, size_t mip_count = 1, size_t array_size = 1, size_t sample_count = 1);
        texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type = ff::dxgi::sprite_type::unknown);
        texture(texture&& other) noexcept;
        texture(const texture& other) = delete;
        virtual ~texture() override;

        texture& operator=(texture&& other) noexcept;
        texture& operator=(const texture& other) = delete;
        operator bool() const;

        ff::dx12::resource* resource() const;

        // texture_base
        virtual ff::dxgi::sprite_type sprite_type() const override;
        virtual std::shared_ptr<DirectX::ScratchImage> data() const override;
        virtual bool update(ff::dxgi::command_context_base& context, size_t array_index, size_t mip_index, const ff::point_size& pos, const DirectX::Image& data) override;

        // texture_metadata_base
        virtual ff::point_size size() const override;
        virtual size_t mip_count() const override;
        virtual size_t array_size() const override;
        virtual size_t sample_count() const override;
        virtual DXGI_FORMAT format() const override;

        // texture_view_base
        virtual const ff::dxgi::texture_view_access_base& view_access() const override;
        virtual const ff::dxgi::texture_base* view_texture() const override;

        // texture_view_access
        virtual D3D12_CPU_DESCRIPTOR_HANDLE dx12_texture_view() const override;

    protected:
        texture();

        texture& assign(texture&& other) noexcept;
        virtual void on_reset();

    private:
        // device_child_base
        virtual bool reset() override;

        mutable std::unique_ptr<ff::dx12::resource> resource_;
        std::shared_ptr<DirectX::ScratchImage> data_;
        ff::dx12::descriptor_range view_;
        ff::dxgi::sprite_type sprite_type_;
    };
}
