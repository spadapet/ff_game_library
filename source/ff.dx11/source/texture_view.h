#pragma once

#include "texture_view_access.h"

namespace ff::dx11
{
    class texture;

    class texture_view : public ff::dxgi::texture_view_base, public ff::dx11::texture_view_access, private ff::dxgi::device_child_base
    {
    public:
        texture_view(const std::shared_ptr<ff::dx11::texture>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_start = 0, size_t mip_count = 0);
        texture_view(texture_view&& other) noexcept;
        texture_view(const texture_view& other) = delete;
        virtual ~texture_view() override;

        texture_view& operator=(texture_view&& other) noexcept;
        texture_view& operator=(const texture_view& other) = delete;
        operator bool() const;

        // texture_view_base
        virtual ff::dxgi::texture_view_access_base& view_access() override;
        virtual ff::dxgi::texture_base* view_texture() override;
        virtual size_t view_array_start() const override;
        virtual size_t view_array_size() const override;
        virtual size_t view_mip_start() const override;
        virtual size_t view_mip_size() const override;

        // texture_view_access
        virtual ID3D11ShaderResourceView* dx11_texture_view() const override;

    private:
        // device_child_base
        virtual bool reset() override;

        mutable Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
        std::shared_ptr<ff::dx11::texture> texture_;
        size_t array_start_;
        size_t array_count_;
        size_t mip_start_;
        size_t mip_count_;
    };
}
