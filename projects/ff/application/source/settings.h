#pragma once

namespace ff
{
    ff::dict settings(std::string_view name);
    void settings(std::string_view name, const ff::dict& dict);
    ff::signal_sink<>& request_save_settings_sink();
}

namespace ff::internal::app
{
    void clear_settings();
    void load_settings();
    bool save_settings();
    void request_save_settings();
}
