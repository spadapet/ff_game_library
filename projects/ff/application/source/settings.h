#pragma once

namespace ff
{
    ff::dict settings(std::string_view name);
    void settings(std::string_view name, const ff::dict& dict);
}

namespace ff::internal::app
{
    void clear_settings();
    void load_settings();
    bool save_settings();
}
