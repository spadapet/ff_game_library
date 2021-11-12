#pragma once

namespace ff::dx11
{
    class depth : public ff::dxgi::depth_base, private ff::dxgi::device_child_base
    {
    public:
        depth(size_t sample_count = 0);
        depth(const ff::point_size& size, size_t sample_count = 0);
        depth(depth&& other) noexcept = default;
        depth(const depth& other) = delete;
        virtual ~depth() override;

        static depth& get(ff::dxgi::depth_base& obj);
        static const depth& get(const ff::dxgi::depth_base& obj);
        depth& operator=(depth&& other) noexcept = default;
        depth& operator=(const depth& other) = delete;
        operator bool() const;

        ID3D11Texture2D* texture() const;
        ID3D11DepthStencilView* view() const;

        // depth_base
        virtual ff::point_size size() const override;
        virtual bool size(const ff::point_size& size) override;
        virtual size_t sample_count() const override;
        virtual void clear(ff::dxgi::command_context_base& context, float depth, BYTE stencil) const override;
        virtual void clear_depth(ff::dxgi::command_context_base& context, float depth = 0.0f) const override;
        virtual void clear_stencil(ff::dxgi::command_context_base& context, BYTE stencil) const override;

    private:
        // device_child_base
        virtual bool reset() override;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> view_;
    };
}
