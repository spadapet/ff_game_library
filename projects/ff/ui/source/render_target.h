#pragma once

namespace ff::internal::ui
{
    class texture;

    class render_target : public Noesis::RenderTarget
    {
    public:
        render_target(size_t width, size_t height, size_t samples, bool srgb, std::string_view name);
        render_target(const render_target& rhs, std::string_view name);

        static ff::internal::ui::render_target* get(Noesis::RenderTarget* target);
        Noesis::Ptr<ff::internal::ui::render_target> clone(std::string_view name) const;
        const std::string& name() const;
        const std::shared_ptr<ff::dx11_texture>& resolved_texture() const;
        const std::shared_ptr<ff::dx11_texture>& msaa_texture() const;
        const std::shared_ptr<ff::dx11_target_base>& resolved_target() const;
        const std::shared_ptr<ff::dx11_target_base>& msaa_target() const;
        const std::shared_ptr<ff::dx11_depth>& depth() const;

        virtual Noesis::Texture* GetTexture() override;

    private:
        std::string name_;
        std::shared_ptr<ff::dx11_texture> resolved_texture_;
        std::shared_ptr<ff::dx11_texture> msaa_texture_;
        std::shared_ptr<ff::dx11_target_base> resolved_target_;
        std::shared_ptr<ff::dx11_target_base> msaa_target_;
        std::shared_ptr<ff::dx11_depth> depth_;
        Noesis::Ptr<ff::internal::ui::texture> resolved_texture_wrapper;
    };
}
