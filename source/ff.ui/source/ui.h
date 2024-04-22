#pragma once

namespace ff
{
    struct init_ui_params;
    class ui_view;
}

namespace ff::ui
{
    const ff::dxgi::palette_base* global_palette();
    const std::vector<ff::ui_view*>& input_views();
    const std::vector<ff::ui_view*>& rendered_views();
    const wchar_t* cursor_resource();

    // <name>:///MainWindow.xaml
    void add_scheme_resources(std::string_view name, std::shared_ptr<ff::resource_object_provider> resources);
    // pack://application:,,,/<name>;component/MainWindow.xaml
    void add_assembly_resources(std::string_view name, std::shared_ptr<ff::resource_object_provider> resources);

    // These notifications must be called at the right time (just use application's ui_state class to do it automatically)
    void state_advance_time();
    void state_advance_input();
    void state_rendering();
    void state_rendered();
}

namespace ff::internal::ui
{
    class render_device;

    bool init(const ff::init_ui_params& params);
    void destroy();

    void init_game_thread(std::function<void()>&& register_extra_components);
    void destroy_game_thread();

    ff::internal::ui::render_device* global_render_device();
    ff::resource_object_provider& shader_resources();

    void register_view(ff::ui_view* view);
    void unregister_view(ff::ui_view* view);
    ff::internal::ui::render_device* on_render_view(ff::ui_view* view);
    void on_focus_view(ff::ui_view* view, bool focused);
}
