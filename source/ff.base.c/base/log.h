#pragma once

#include "../base/string.h"

typedef enum
{
    ff_log_type_none, // not visible by default
    ff_log_type_normal, // generic game/app output
    ff_log_type_debug, // debug build only

    ff_log_type_count,
} ff_log_type;

typedef void (*ff_log_sink_func)(ff_log_type type, ff_string_view text, void* cookie);

typedef struct ff_log_sink_data
{
    ff_log_sink_func sink;
    void* cookie;
} ff_log_sink_data;

ff_log_sink_data ff_log_set_sink(ff_log_sink_data sink);

ff_string_view ff_log_get_type_name(ff_log_type type);
bool ff_log_get_type_enabled(ff_log_type type);
void ff_log_set_type_enabled(ff_log_type type, bool value);

void ff_log_write(ff_log_type type, ff_string_view format, ...);
void ff_log_write_v(ff_log_type type, ff_string_view format, va_list args);
