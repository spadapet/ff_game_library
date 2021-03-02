#pragma once

namespace ff::ui
{
}

namespace ff::internal::ui
{
    class font_provider;
    class resource_cache;
    class texture_provider;
    class xaml_provider;

    bool init();
    void destroy();

    ff::internal::ui::font_provider* global_font_provider();
    ff::internal::ui::resource_cache* global_resource_cache();
    ff::internal::ui::texture_provider* global_texture_provider();
    ff::internal::ui::xaml_provider* global_xaml_provider();
}
