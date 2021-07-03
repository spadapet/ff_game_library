#pragma once

namespace ff
{
    class ui_view
    {
    public:
        ui_view(std::string_view xaml_file, bool per_pixel_anti_alias = false, bool sub_pixel_rendering = false);
        ui_view(Noesis::FrameworkElement* content, bool per_pixel_anti_alias = false, bool sub_pixel_rendering = false);
        virtual ~ui_view();

        void destroy();
        virtual void advance();
        virtual void pre_render();
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth, const ff::rect_float* view_rect = nullptr);

        Noesis::IView* internal_view() const;
        Noesis::FrameworkElement* content() const;
        Noesis::Visual* hit_test(ff::point_float screen_pos) const;
        Noesis::Cursor cursor() const;
        void cursor(Noesis::Cursor cursor);
        void size(const ff::window_size& value);
        void size(ff::target_window_base& target);
        ff::point_float screen_to_content(ff::point_float pos) const;
        ff::point_float content_to_screen(ff::point_float pos) const;

        void set_view_to_screen_transform(ff::point_float pos, ff::point_float scale);
        void set_view_to_screen_transform(const DirectX::XMMATRIX& matrix);
        ff::point_float screen_to_view(ff::point_float pos) const;
        ff::point_float view_to_screen(ff::point_float pos) const;

        void focused(bool focus);
        bool focused() const;
        void enabled(bool value);
        bool enabled() const;
        void block_input_below(bool block);
        bool block_input_below() const;

    protected:
        virtual bool render_begin(ff::dx11_target_base& target, ff::dx11_depth& depth, const ff::rect_float* view_rect);

    private:
        DirectX::XMMATRIX* matrix;
        bool focused_;
        bool enabled_;
        bool block_input_below_;
        double counter;
        ff::signal_connection target_size_changed;
        ff::window_size current_size;
        Noesis::Cursor cursor_;
        Noesis::Ptr<Noesis::Grid> view_grid;
        Noesis::Ptr<Noesis::Viewbox> view_box;
        Noesis::Ptr<Noesis::IView> internal_view_;
        Noesis::Ptr<Noesis::RotateTransform> rotate_transform;
        Noesis::Ptr<Noesis::FrameworkElement> content_;
    };
}
