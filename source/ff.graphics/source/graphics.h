#pragma once

namespace ff
{
    class target_base;
    class target_window_base;
}

namespace ff::graphics
{
    IDWriteFactoryX* write_factory();
    IDWriteInMemoryFontFileLoader* write_font_loader();
}

namespace ff::graphics::defer
{
    void set_full_screen_target(ff::target_window_base* target);
    void remove_target(ff::target_window_base* target);
    void resize_target(ff::target_window_base* target, const ff::window_size& size);
    void validate_device(bool force);
    void full_screen(bool value);
}

namespace ff::internal::graphics
{
    bool init();
    void destroy();
}
