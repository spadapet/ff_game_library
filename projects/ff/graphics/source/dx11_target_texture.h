#pragma once

#include "graphics_child_base.h"
#include "target_base.h"

namespace ff
{
    class texture;

    class dx11_target_texture
        : public ff::target_base
        , public ff::internal::graphics_child_base
    {
    public:
        dx11_target_texture(ff::texture&& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0);
        dx11_target_texture(const std::shared_ptr<ff::texture>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0);
        dx11_target_texture(dx11_target_texture&& other) noexcept = default;
        dx11_target_texture(const dx11_target_texture& other) = delete;
        virtual ~dx11_target_texture() override;

        dx11_target_texture& operator=(dx11_target_texture&& other) noexcept = default;
        dx11_target_texture& operator=(const dx11_target_texture & other) = delete;
        operator bool() const;

        const std::shared_ptr<ff::texture>& shared_texture() const;

        // target_base
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;
        virtual ID3D11Texture2D* texture() override;
        virtual ID3D11RenderTargetView* view() override;

        // graphics_child_base
        virtual bool reset() override;

    private:
        std::shared_ptr<ff::texture> texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
        size_t array_start;
        size_t array_count;
        size_t mip_level;
    };
}
