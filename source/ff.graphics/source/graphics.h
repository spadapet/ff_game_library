#pragma once

namespace ff
{
    const ff::dxgi::client_functions& dxgi_client();
}

namespace ff::graphics
{
    IDWriteFactoryX* write_factory();
    IDWriteInMemoryFontFileLoader* write_font_loader();
}

namespace ff::graphics::defer
{
    void set_full_screen_target(ff::dxgi::target_window_base* target);
    void remove_target(ff::dxgi::target_window_base* target);
    void resize_target(ff::dxgi::target_window_base* target, const ff::window_size& size);
    void reset_device(bool force);
    void full_screen(bool value);
}

namespace ff::internal::graphics
{
    bool init(const ff::dxgi::client_functions& client_functions);
    void destroy();
    void frame_started();
    void frame_complete();
}
