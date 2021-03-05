#pragma once

namespace ff
{
    struct init_ui_params;
    class ui_view;
}

namespace ff::ui
{
    const ff::palette_base* global_palette();
    const std::vector<ff::ui_view*>& input_views();
    const std::vector<ff::ui_view*>& rendered_views();
}

namespace ff::internal::ui
{
    class font_provider;
    class render_device;
    class resource_cache;
    class texture_provider;
    class xaml_provider;

    bool init(const ff::init_ui_params& params);
    void destroy();

    ff::internal::ui::font_provider* global_font_provider();
    ff::internal::ui::render_device* global_render_device();
    ff::internal::ui::resource_cache* global_resource_cache();
    ff::internal::ui::texture_provider* global_texture_provider();
    ff::internal::ui::xaml_provider* global_xaml_provider();

    void register_view(ff::ui_view* view);
    void unregister_view(ff::ui_view* view);
    void on_render_view(ff::ui_view* view);
    void on_focus_view(ff::ui_view* view, bool focused);

    // These notifications must be called at the right time (just use application's ui_state class to do it automatically)
    void state_advance();
    void state_advance_input();
    void state_rendering();
    void state_rendered();
}
