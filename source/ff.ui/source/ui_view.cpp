#include "pch.h"
#include "render_device.h"
#include "ui.h"
#include "ui_view.h"

ff::ui_view::ui_view(std::string_view xaml_file, bool per_pixel_anti_alias, bool sub_pixel_rendering)
    : ui_view(Noesis::GUI::LoadXaml<Noesis::FrameworkElement>(std::string(xaml_file).c_str()), per_pixel_anti_alias, sub_pixel_rendering)
{}

ff::ui_view::ui_view(Noesis::FrameworkElement* content, bool per_pixel_anti_alias, bool sub_pixel_rendering)
    : matrix(new DirectX::XMMATRIX[2])
    , focused_(false)
    , enabled_(true)
    , block_input_below_(false)
    , counter(0)
    , current_size{}
    , cursor_(Noesis::Cursor_Arrow)
    , view_grid(Noesis::MakePtr<Noesis::Grid>())
    , view_box(Noesis::MakePtr<Noesis::Viewbox>())
    , internal_view_(Noesis::GUI::CreateView(this->view_box))
    , rotate_transform(Noesis::MakePtr<Noesis::RotateTransform>())
    , content_(content)
{
    assert(content);

    this->matrix[0] = DirectX::XMMatrixIdentity();
    this->matrix[1] = DirectX::XMMatrixIdentity();

    ff::internal::ui::register_view(this);

    this->internal_view_->SetFlags((per_pixel_anti_alias ? Noesis::RenderFlags_PPAA : 0) | (sub_pixel_rendering ? Noesis::RenderFlags_LCD : 0));
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
    delete[] this->matrix;
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
    ff::point_float pos = screen_to_content(screen_pos);
    Noesis::HitTestResult ht = Noesis::VisualTreeHelper::HitTest(this->content_, Noesis::Point(pos.x, pos.y));
    return ht.visualHit;
}

Noesis::Cursor ff::ui_view::cursor() const
{
    return this->cursor_;
}

void ff::ui_view::cursor(Noesis::Cursor cursor)
{
    this->cursor_ = cursor;
}

void ff::ui_view::size(const ff::window_size& value)
{
    if (value != this->current_size)
    {
        ff::point_float dip_size = value.rotated_pixel_size().cast<float>() / static_cast<float>(value.dpi_scale);

        this->current_size = value;
        this->rotate_transform->SetAngle(static_cast<float>(value.rotated_degrees_from_native()));
        this->view_grid->SetWidth(dip_size.x);
        this->view_grid->SetHeight(dip_size.y);
        this->internal_view_->SetSize(static_cast<uint32_t>(value.pixel_size.x), static_cast<uint32_t>(value.pixel_size.y));
    }
}

void ff::ui_view::size(ff::target_window_base& target)
{
    this->size(target.size());

    this->target_size_changed = target.size_changed().connect([this](ff::window_size target_size)
        {
            this->size(target_size);
        });
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
    return view_to_screen(ff::point_float(viewPos.x, viewPos.y));
}

void ff::ui_view::set_view_to_screen_transform(ff::point_float pos, ff::point_float scale)
{
    this->set_view_to_screen_transform(DirectX::XMMatrixAffineTransformation2D(
        DirectX::XMVectorSet(scale.x, scale.y, 1, 1), // scale
        DirectX::XMVectorSet(0, 0, 0, 0), // rotation center
        0, // rotation
        DirectX::XMVectorSet(pos.x, pos.y, 0, 0))); // translation
}

void ff::ui_view::set_view_to_screen_transform(const DirectX::XMMATRIX& matrix)
{
    if (matrix != this->matrix[0])
    {
        this->matrix[0] = matrix;
        this->matrix[1] = DirectX::XMMatrixInverse(nullptr, this->matrix[0]);
    }
}

ff::point_float ff::ui_view::screen_to_view(ff::point_float pos) const
{
    DirectX::XMFLOAT2 viewPos;
    DirectX::XMStoreFloat2(&viewPos, DirectX::XMVector2Transform(DirectX::XMVectorSet(pos.x, pos.y, 0, 0), this->matrix[1]));
    return ff::point_float(viewPos.x, viewPos.y);
}

ff::point_float ff::ui_view::view_to_screen(ff::point_float pos) const
{
    DirectX::XMFLOAT2 screen_pos;
    DirectX::XMStoreFloat2(&screen_pos, DirectX::XMVector2Transform(DirectX::XMVectorSet(pos.x, pos.y, 0, 0), this->matrix[0]));
    return ff::point_float(screen_pos.x, screen_pos.y);
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

    if (this->internal_view_->Update(time))
    {
        this->internal_view_->GetRenderer()->UpdateRenderTree();
    }
}

void ff::ui_view::pre_render()
{
    this->internal_view_->GetRenderer()->RenderOffscreen();
}

void ff::ui_view::render(ff::target_base& target, ff::depth& depth, const ff::rect_float* view_rect)
{
    ff::internal::ui::on_render_view(this);

    if (this->render_begin(target, depth, view_rect))
    {
        this->internal_view_->GetRenderer()->Render();
    }
}

bool ff::ui_view::render_begin(ff::target_base& target, ff::depth& depth, const ff::rect_float* view_rect)
{
    if (depth.size(target.size().pixel_size))
    {
        ID3D11RenderTargetView* target_view = target.view();
        ID3D11DepthStencilView* depth_view = depth.view();

        if (target_view && depth_view)
        {
            D3D11_VIEWPORT viewport{};
            viewport.MaxDepth = 1;

            if (view_rect)
            {
                viewport.TopLeftX = view_rect->left;
                viewport.TopLeftY = view_rect->top;
                viewport.Width = view_rect->width();
                viewport.Height = view_rect->height();
            }
            else
            {
                ff::point_float target_size = target.size().pixel_size.cast<float>();
                viewport.Width = target_size.x;
                viewport.Height = target_size.y;
            }

            ff::graphics::dx11_device_state().set_targets(&target_view, 1, depth_view);
            ff::graphics::dx11_device_state().set_viewports(&viewport, 1);

            return true;
        }
    }

    assert(false);
    return false;
}
