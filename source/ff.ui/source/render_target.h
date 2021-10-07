#pragma once

namespace ff::internal::ui
{
    class texture;

    class render_target : public Noesis::RenderTarget
    {
    public:
        render_target(size_t width, size_t height, size_t samples, bool srgb, bool needs_depth_stencil, std::string_view name);
        render_target(const render_target& rhs, std::string_view name);

        static ff::internal::ui::render_target* get(Noesis::RenderTarget* target);
        Noesis::Ptr<ff::internal::ui::render_target> clone(std::string_view name) const;
        const std::string& name() const;
        const std::shared_ptr<ff::texture>& resolved_texture() const;
        const std::shared_ptr<ff::texture>& msaa_texture() const;
        const std::shared_ptr<ff::target_base>& resolved_target() const;
        const std::shared_ptr<ff::target_base>& msaa_target() const;
        const std::shared_ptr<ff::dxgi::depth_base>& depth() const;

        virtual Noesis::Texture* GetTexture() override;

    private:
        std::string name_;
        std::shared_ptr<ff::texture> resolved_texture_;
        std::shared_ptr<ff::texture> msaa_texture_;
        std::shared_ptr<ff::target_base> resolved_target_;
        std::shared_ptr<ff::target_base> msaa_target_;
        std::shared_ptr<ff::dxgi::depth_base> depth_;
        Noesis::Ptr<ff::internal::ui::texture> resolved_texture_wrapper;
    };
}
