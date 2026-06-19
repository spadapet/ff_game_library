#pragma once

#include "../base/string.h"

namespace ff::log
{
    // Logging categories. Each maps to a fixed name and a default-enabled flag (see log.cpp). The
    // value is a stable index into a flat table, so add new entries before 'count' and avoid
    // reordering if persisted enable/disable settings ever matter. 'count' is a sentinel that sizes
    // the table; it is not a usable category.
    enum class type
    {
        none, // not visible by default
        normal, // generic game/app output
        debug, // debug build only

        count,
    };

    // A sink receives each fully-formatted line (prefix + message + "\r\n"). The app installs one to
    // route logs somewhere durable (e.g., a file); returns the previously installed sink. Pass nullptr
    // to remove. Debug builds also write to the debugger output independently of any sink.
    typedef void (*sink_func)(ff::string_view text);
    ff::log::sink_func sink(ff::log::sink_func sink);

    ff::string_view type_name(ff::log::type type);
    bool type_enabled(ff::log::type type);
    void type_enabled(ff::log::type type, bool value);

    // Write a printf-formatted line under 'type'; a no-op when the type is disabled. The message is
    // prefixed with "[type_name] " and suffixed with "\r\n". 'format' is a UTF-8 string_view.
    void write(ff::log::type type, ff::string_view format, ...);
    void write_v(ff::log::type type, ff::string_view format, va_list args);
}
