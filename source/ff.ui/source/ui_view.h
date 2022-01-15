#pragma once

namespace ff
{
    enum class ui_view_options
    {
        none,
        per_pixel_anti_alias = 0x01,
        sub_pixel_rendering = 0x02,
        cache_render = 0x04,
    };

    class ui_view
    {
    public:
        ui_view(std::string_view xaml_file, ff::ui_view_options options = ff::ui_view_options::none);
        ui_view(Noesis::FrameworkElement* content, ff::ui_view_options options = ff::ui_view_options::none);
        ~ui_view();

        void destroy();
        void advance();
        void render(ff::dxgi::target_base& target, ff::dxgi::depth_base& depth);
        ff::signal_sink<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&>& rendering();
        ff::signal_sink<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&>& rendered();

        Noesis::IView* internal_view() const;
        Noesis::FrameworkElement* content() const;
        Noesis::Visual* hit_test(ff::point_float screen_pos) const;
        Noesis::CursorType cursor() const;
        void cursor(Noesis::CursorType cursor);
        void size(const ff::window_size& value);
        void size(ff::dxgi::target_window_base& target);

        ff::point_float screen_to_content(ff::point_float pos) const;
        ff::point_float content_to_screen(ff::point_float pos) const;
        ff::point_float screen_to_view(ff::point_float pos) const;
        ff::point_float view_to_screen(ff::point_float pos) const;

        void focused(bool focus);
        bool focused() const;
        void enabled(bool value);
        bool enabled() const;
        void block_input_below(bool block);
        bool block_input_below() const;

    private:
        void internal_size(const ff::window_size& value);

        bool focused_;
        bool enabled_;
        bool block_input_below_;
        bool update_render;
        bool cache_render;
        double counter;

        ff::signal_connection target_size_changed;
        ff::signal<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&> rendering_;
        ff::signal<ff::ui_view*, ff::dxgi::target_base&, ff::dxgi::depth_base&> rendered_;
        ff::window_size current_size;
        std::shared_ptr<ff::texture> cache_texture;
        std::shared_ptr<ff::dxgi::target_base> cache_target;

        Noesis::CursorType cursor_;
        Noesis::Ptr<Noesis::Grid> view_grid;
        Noesis::Ptr<Noesis::Viewbox> view_box;
        Noesis::Ptr<Noesis::IView> internal_view_;
        Noesis::Ptr<Noesis::RotateTransform> rotate_transform;
        Noesis::Ptr<Noesis::FrameworkElement> content_;
    };
}
