#pragma once

namespace ff::internal::imgui
{
    void init(
        ff::window* window,
        std::shared_ptr<ff::dxgi::target_window_base> app_target,
        std::shared_ptr<ff::resource_object_provider> app_resources);
    void destroy();
    void advance_input();
    void rendering();
    void render(ff::dxgi::command_context_base& context);
    void rendered();
    bool handle_window_message(ff::window* window, ff::window_message& message);
}
