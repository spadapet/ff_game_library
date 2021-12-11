#pragma once

#include "texture_view_access.h"

namespace ff::dx11
{
    class texture : public ff::dxgi::texture_base, public ff::dx11::texture_view_access, private ff::dxgi::device_child_base
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

        static ff::dx11::texture& get(ff::dxgi::texture_base& obj);
        ID3D11Texture2D* dx11_texture() const;

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
        virtual ff::dxgi::texture_view_access_base& view_access() override;
        virtual ff::dxgi::texture_base* view_texture() override;

        // texture_view_access
        virtual ID3D11ShaderResourceView* dx11_texture_view() const override;

    protected:
        texture();

        texture& assign(texture&& other) noexcept;
        virtual void on_reset();

    private:
        // device_child_base
        virtual bool reset() override;

        mutable Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        mutable Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<DirectX::ScratchImage> data_;
        ff::dxgi::sprite_type sprite_type_;
    };
}
