#include "pch.h"
#include "render_device.h"
#include "ui.h"
#include "ui_view.h"

ff::ui_view::ui_view(std::string_view xaml_file, ff::ui_view_options options)
    : ui_view(Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(std::string(xaml_file).c_str()), options)
{}

ff::ui_view::ui_view(Noesis::FrameworkElement* content, ff::ui_view_options options)
    : focused_(false)
    , enabled_(true)
    , block_input_below_(false)
    , update_render(false)
    , cache_render(ff::flags::has(options, ff::ui_view_options::cache_render))
    , counter(0)
    , current_size{}
    , cursor_(Noesis::CursorType_Arrow)
    , view_grid(Noesis::MakePtr<Noesis::Grid>())
    , view_box(Noesis::MakePtr<Noesis::Viewbox>())
    , internal_view_(Noesis::GUI::CreateView(this->view_box))
    , rotate_transform(Noesis::MakePtr<Noesis::RotateTransform>())
    , content_(content)
{
    assert(content);

    ff::internal::ui::register_view(this);

    this->internal_view_->SetFlags(
        (ff::flags::has(options, ff::ui_view_options::per_pixel_anti_alias) ? Noesis::RenderFlags_PPAA : 0) |
        (ff::flags::has(options, ff::ui_view_options::sub_pixel_rendering) ? Noesis::RenderFlags_LCD : 0));
    this->internal_view_->GetRenderer()->Init(ff::internal::ui::global_render_device());
    this->internal_view_->Deactivate();

    assert(!this->content_->GetParent());

    this->view_grid->SetLayoutTransform(this->rotate_transform);
    this->view_grid->GetChildren()->Add(this->content_);
    this->view_box->SetStretchDirection(Noesis::StretchDirection::StretchDirection_Both);
    this->view_box->SetStretch(Noesis::Stretch::Stretch_Fill);
    this->view_box->SetChild(this->view_grid);
}

ff::ui_view::~ui_view()
{
    this->destroy();
}

void ff::ui_view::destroy()
{
    if (this->internal_view_)
    {
        this->internal_view_->GetRenderer()->Shutdown();
        this->internal_view_ = nullptr;
    }

    ff::internal::ui::unregister_view(this);
}

Noesis::IView* ff::ui_view::internal_view() const
{
    return this->internal_view_;
}

Noesis::FrameworkElement* ff::ui_view::content() const
{
    return this->content_;
}

Noesis::Visual* ff::ui_view::hit_test(ff::point_float screen_pos) const
{
    ff::point_float pos = this->screen_to_content(screen_pos);
    Noesis::HitTestResult ht = Noesis::VisualTreeHelper::HitTest(this->content_, Noesis::Point(pos.x, pos.y));
    return ht.visualHit;
}

Noesis::CursorType ff::ui_view::cursor() const
{
    return this->cursor_;
}

void ff::ui_view::cursor(Noesis::CursorType cursor)
{
    this->cursor_ = cursor;
}

void ff::ui_view::size(const ff::window_size& value)
{
    this->target_size_changed.disconnect();
    this->internal_size(value);
}

void ff::ui_view::size(ff::dxgi::target_window_base& target)
{
    this->internal_size(target.size());

    this->target_size_changed = target.size_changed().connect([this](ff::window_size target_size)
    {
        this->internal_size(target_size);
    });
}

void ff::ui_view::internal_size(const ff::window_size& value)
{
    if (value != this->current_size)
    {
        const float dpi_scale = static_cast<float>(value.dpi_scale);
        const float rotate_degrees_cw = static_cast<float>((360 - value.rotated_degrees_from_native()) % 360);
        const ff::point_float dip_size = value.pixel_size.cast<float>() / dpi_scale;
        const ff::point_t<uint32_t> rotated_pixel_size = value.rotated_pixel_size().cast<uint32_t>();

        // Size for Grid that owns the content and gets rotated
        this->view_grid->SetWidth(dip_size.x);
        this->view_grid->SetHeight(dip_size.y);
        this->rotate_transform->SetAngle(rotate_degrees_cw);

        // Size for internal Noesis view
        this->internal_view_->SetSize(rotated_pixel_size.x, rotated_pixel_size.y);
        this->internal_view_->SetScale(dpi_scale);

        this->current_size = value;
    }
}

ff::point_float ff::ui_view::screen_to_content(ff::point_float pos) const
{
    pos = this->screen_to_view(pos);
    Noesis::Point pos2 = this->content_->PointFromScreen(Noesis::Point(pos.x, pos.y));
    return ff::point_float(pos2.x, pos2.y);
}

ff::point_float ff::ui_view::content_to_screen(ff::point_float pos) const
{
    Noesis::Point viewPos = this->content_->PointToScreen(Noesis::Point(pos.x, pos.y));
    return this->view_to_screen(ff::point_float(viewPos.x, viewPos.y));
}

ff::point_float ff::ui_view::screen_to_view(ff::point_float pos) const
{
    return this->current_size.rotate_point(pos);
}

ff::point_float ff::ui_view::view_to_screen(ff::point_float pos) const
{
    return this->current_size.unrotate_point(pos);
}

void ff::ui_view::focused(bool focus)
{
    if (this->focused_ != focus)
    {
        if (focus)
        {
            this->focused_ = true;
            this->internal_view_->Activate();
        }
        else
        {
            this->focused_ = false;
            this->internal_view_->Deactivate();
        }

        ff::internal::ui::on_focus_view(this, focus);
    }
}

bool ff::ui_view::focused() const
{
    return this->focused_;
}

void ff::ui_view::enabled(bool enabled)
{
    this->enabled_ = enabled;
}

bool ff::ui_view::enabled() const
{
    return this->enabled_;
}

void ff::ui_view::block_input_below(bool block)
{
    this->block_input_below_ = block;
}

bool ff::ui_view::block_input_below() const
{
    return this->block_input_below_;
}

void ff::ui_view::advance()
{
    double time = this->counter++ * ff::constants::seconds_per_advance;
    this->update_render = this->internal_view_->Update(time);
}

void ff::ui_view::render(ff::dxgi::target_base& target, ff::dxgi::depth_base& depth)
{
    ff::internal::ui::render_device& render_device = *ff::internal::ui::on_render_view(this);
    const ff::window_size target_size = target.size();
    const ff::point_size rotated_pixel_size = target_size.rotated_pixel_size();
    const ff::rect_size rotated_pixel_rect({}, rotated_pixel_size);

    if (this->update_render)
    {
        this->internal_view_->GetRenderer()->UpdateRenderTree();

        render_device.render_begin();
        this->internal_view_->GetRenderer()->RenderOffscreen();
        render_device.render_end();

        if (this->cache_render && (!this->cache_target || this->cache_target->size() != target_size))
        {
            this->cache_target.reset();

            if (!this->cache_texture || this->cache_texture->dxgi_texture()->size() != rotated_pixel_size)
            {
                this->cache_texture.reset();
                const DXGI_FORMAT cache_format = render_device.GetCaps().linearRendering ? ff::dxgi::DEFAULT_FORMAT_SRGB : ff::dxgi::DEFAULT_FORMAT;
                auto dxgi_texture = ff::dxgi_client().create_render_texture(rotated_pixel_size, cache_format, 1, 1, 1, &ff::dxgi::color_none());
                this->cache_texture = std::make_shared<ff::texture>(dxgi_texture);
            }

            this->cache_target = ff::dxgi_client().create_target_for_texture(
                this->cache_texture->dxgi_texture(), 0, 0, 0,
                target_size.native_rotation, target_size.current_rotation, target_size.dpi_scale);
        }
    }

    ff::dxgi::target_base& render_target = this->cache_target ? *this->cache_target : target;

    if ((this->update_render || !this->cache_render) && depth.size(rotated_pixel_size))
    {
        this->rendering_.notify(this, render_target, depth);
        ff::dxgi::command_context_base& context = render_device.render_begin(render_target, depth, rotated_pixel_rect);

        if (this->cache_target)
        {
            this->cache_target->clear(context, ff::dxgi::color_none());
        }

        this->internal_view_->GetRenderer()->Render();
        render_device.render_end();
        this->rendered_.notify(this, render_target, depth);
    }

    if (this->cache_texture)
    {
        ff::dxgi::draw_ptr draw = ff::dxgi_client().global_draw_device().begin_draw(
            target, nullptr, rotated_pixel_rect.cast<float>(), rotated_pixel_rect.cast<float>(),
            ff::flags::combine(ff::dxgi::draw_options::pre_multiplied_alpha, ff::dxgi::draw_options::ignore_orientation));

        if (draw)
        {
            draw->draw_sprite(this->cache_texture->sprite_data(), ff::dxgi::transform::identity());
        }
    }

    this->update_render = false;
}

ff::signal_sink<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&>& ff::ui_view::rendering()
{
    return this->rendering_;
}

ff::signal_sink<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&>& ff::ui_view::rendered()
{
    return this->rendered_;
}
