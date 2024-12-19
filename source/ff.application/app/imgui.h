#pragma once

namespace ff::internal::imgui
{
    void init(ff::window* window);
    void destroy();
    void advance_input();
    void rendering();
    void render(ff::dxgi::command_context_base& context);
    void rendered();
    bool handle_window_message(ff::window* window, ff::window_message& message);
}
