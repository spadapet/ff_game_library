#include "pch.h"
#include "texture.h"
#include "render_target.h"

ff::internal::ui::render_target::render_target(size_t width, size_t height, size_t samples, bool srgb, bool needs_depth_stencil, std::string_view name)
    : name_(name)
{
    ff::point_size size(width, height);
    DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

    auto msaa_dxgi_texture = ff::dxgi_client().create_render_texture(size, format, 1, 1, samples, nullptr);
    this->msaa_texture_ = std::make_shared<ff::texture>(msaa_dxgi_texture);
    this->msaa_target_ = ff::dxgi_client().create_target_for_texture(this->msaa_texture_->dxgi_texture());

    if (this->msaa_texture_->dxgi_texture()->sample_count() > 1)
    {
        auto resolved_dxgi_texture = ff::dxgi_client().create_render_texture(size, format, 1, 1, 1, nullptr);
        this->resolved_texture_ = std::make_shared<ff::texture>(resolved_dxgi_texture);
        this->resolved_target_ = ff::dxgi_client().create_target_for_texture(this->resolved_texture_->dxgi_texture());
    }
    else
    {
        this->resolved_texture_ = this->msaa_texture_;
        this->resolved_target_ = this->msaa_target_;
    }

    this->resolved_texture_wrapper = Noesis::MakePtr<ff::internal::ui::texture>(this->resolved_texture_, name);

    if (needs_depth_stencil)
    {
        this->depth_ = ff::dxgi_client().create_depth(size, this->msaa_texture_->dxgi_texture()->sample_count());
    }
}

ff::internal::ui::render_target::render_target(const render_target& rhs, std::string_view name)
    : name_(name)
    , depth_(rhs.depth_)
{
    auto resolved_dxgi_texture = ff::dxgi_client().create_render_texture(rhs.resolved_texture_->dxgi_texture()->size(), rhs.resolved_texture_->dxgi_texture()->format(), 1, 1, 1, nullptr);
    this->resolved_texture_ = std::make_shared<ff::texture>(resolved_dxgi_texture);
    this->resolved_target_ = ff::dxgi_client().create_target_for_texture(this->resolved_texture_->dxgi_texture());
    this->resolved_texture_wrapper = Noesis::MakePtr<ff::internal::ui::texture>(this->resolved_texture_, name);

    if (rhs.msaa_texture_->dxgi_texture()->sample_count() > 1)
    {
        this->msaa_texture_ = rhs.msaa_texture_;
        this->msaa_target_ = rhs.msaa_target_;
    }
    else
    {
        this->msaa_texture_ = this->resolved_texture_;
        this->msaa_target_ = this->resolved_target_;
    }
}

ff::internal::ui::render_target* ff::internal::ui::render_target::get(Noesis::RenderTarget* target)
{
    return static_cast<ff::internal::ui::render_target*>(target);
}

Noesis::Ptr<ff::internal::ui::render_target> ff::internal::ui::render_target::clone(std::string_view name) const
{
    return *new ff::internal::ui::render_target(*this, name);
}

const std::string& ff::internal::ui::render_target::name() const
{
    return this->name_;
}

const std::shared_ptr<ff::texture>& ff::internal::ui::render_target::resolved_texture() const
{
    return this->resolved_texture_;
}

const std::shared_ptr<ff::texture>& ff::internal::ui::render_target::msaa_texture() const
{
    return this->msaa_texture_;
}

const std::shared_ptr<ff::dxgi::target_base>& ff::internal::ui::render_target::resolved_target() const
{
    return this->resolved_target_;
}

const std::shared_ptr<ff::dxgi::target_base>& ff::internal::ui::render_target::msaa_target() const
{
    return this->msaa_target_;
}

const std::shared_ptr<ff::dxgi::depth_base>& ff::internal::ui::render_target::depth() const
{
    return this->depth_;
}

Noesis::Texture* ff::internal::ui::render_target::GetTexture()
{
    return this->resolved_texture_wrapper;
}
