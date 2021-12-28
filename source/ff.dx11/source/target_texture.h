#pragma once

#include "target_access.h"

namespace ff::dx11
{
    class texture;

    class target_texture : public ff::dxgi::target_base, public ff::dx11::target_access, private ff::dxgi::device_child_base
    {
    public:
        target_texture(const std::shared_ptr<ff::dx11::texture>& texture, size_t array_start = 0, size_t array_count = 0, size_t mip_level = 0);
        target_texture(target_texture&& other) noexcept;
        target_texture(const target_texture& other) = delete;
        virtual ~target_texture() override;

        target_texture& operator=(target_texture&& other) noexcept = default;
        target_texture& operator=(const target_texture & other) = delete;
        operator bool() const;

        const std::shared_ptr<ff::dx11::texture>& shared_texture() const;

        // target_base
        virtual void clear(ff::dxgi::command_context_base& context, const DirectX::XMFLOAT4& clear_color) override;
        virtual bool pre_render(const DirectX::XMFLOAT4* clear_color) override;
        virtual bool present() override;
        virtual ff::signal_sink<ff::dxgi::target_base*>& render_presented() override;
        virtual ff::dxgi::target_access_base& target_access() override;
        virtual size_t target_array_start() const override;
        virtual size_t target_array_size() const override;
        virtual size_t target_mip_start() const override;
        virtual size_t target_mip_size() const override;
        virtual DXGI_FORMAT format() const override;
        virtual ff::window_size size() const override;

        // target_access
        virtual ID3D11Texture2D* dx11_target_texture() override;
        virtual ID3D11RenderTargetView* dx11_target_view() override;

    private:
        // device_child_base
        virtual bool reset() override;

        std::shared_ptr<ff::dx11::texture> texture_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view_;
        ff::signal<ff::dxgi::target_base*> render_presented_;
        size_t array_start;
        size_t array_count;
        size_t mip_level;
    };
}
