#include "pch.h"
#include "render_target.h"
#include "texture.h"

ff::internal::ui::render_target::render_target(size_t width, size_t height, size_t samples, bool srgb, bool needs_depth_stencil, std::string_view name)
    : name_(name)
{
    ff::point_int size = ff::point_size(width, height).cast<int>();
    DXGI_FORMAT format = srgb ? ff::internal::DEFAULT_FORMAT_SRGB : ff::internal::DEFAULT_FORMAT;

    this->msaa_texture_ = std::make_shared<ff::dx11_texture>(size, format, 1, 1, samples);
    this->msaa_target_ = std::make_shared<ff::dx11_target_texture>(this->msaa_texture_);

    if (this->msaa_texture_->sample_count() > 1)
    {
        this->resolved_texture_ = std::make_shared<ff::dx11_texture>(size, format);
        this->resolved_target_ = std::make_shared<ff::dx11_target_texture>(this->resolved_texture_);
    }
    else
    {
        this->resolved_texture_ = this->msaa_texture_;
        this->resolved_target_ = this->msaa_target_;
    }

    this->resolved_texture_wrapper = Noesis::MakePtr<ff::internal::ui::texture>(this->resolved_texture_, name);

    if (needs_depth_stencil)
    {
        this->depth_ = std::make_shared<ff::dx11_depth>(size, this->msaa_texture_->sample_count());
    }
}

ff::internal::ui::render_target::render_target(const render_target& rhs, std::string_view name)
    : name_(name)
    , depth_(rhs.depth_)
{
    this->resolved_texture_ = std::make_shared<ff::dx11_texture>(rhs.resolved_texture_->size(), rhs.resolved_texture_->format());
    this->resolved_target_ = std::make_shared<ff::dx11_target_texture>(this->resolved_texture_);
    this->resolved_texture_wrapper = Noesis::MakePtr<ff::internal::ui::texture>(this->resolved_texture_, name);

    if (rhs.msaa_texture_->sample_count() > 1)
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

const std::shared_ptr<ff::dx11_texture>& ff::internal::ui::render_target::resolved_texture() const
{
    return this->resolved_texture_;
}

const std::shared_ptr<ff::dx11_texture>& ff::internal::ui::render_target::msaa_texture() const
{
    return this->msaa_texture_;
}

const std::shared_ptr<ff::target_base>& ff::internal::ui::render_target::resolved_target() const
{
    return this->resolved_target_;
}

const std::shared_ptr<ff::target_base>& ff::internal::ui::render_target::msaa_target() const
{
    return this->msaa_target_;
}

const std::shared_ptr<ff::dx11_depth>& ff::internal::ui::render_target::depth() const
{
    return this->depth_;
}

Noesis::Texture* ff::internal::ui::render_target::GetTexture()
{
    return this->resolved_texture_wrapper;
}
