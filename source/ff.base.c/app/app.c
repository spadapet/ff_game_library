#include "pch.h"
#include "app/app.h"
#include "base/arena.h"
#include "base/assert.h"
#include "base/log.h"
#include "base/string.h"
#include "base/string_builder.h"
#include "data/file.h"
#include "data/settings.h"
#include "data/stream.h"
#include "windows/dispatch.h"
#include "windows/task.h"
#include "windows/window.h"

static ff_arena s_arena;
static ff_stream s_log_stream;
static ff_log_sink_data s_old_log_sink;
static ff_dispatch s_main_dispatch;
static ff_window s_main_window;
static bool s_has_log_stream;
static bool s_valid;

static void log_sink(ff_log_type type, ff_string_view text, void* cookie)
{
    ff_stream_write((ff_stream*)cookie, ff_string_view_span(text));
}

static ff_string_view user_file_path(ff_string_view app_name, ff_string_view file_name)
{
    const ff_string_view user_path = ff_file_user_local_path(&s_arena);
    FF_CHECK_RET_VAL(user_path.count, ff_string_view_empty());

    ff_string_builder sb;
    ff_string_builder_init(&sb, &s_arena);
    ff_string_builder_append_format(&sb, FF_SVL("%.*s\\%.*s\\%.*s"),
        FF_SV_FORMAT(user_path),
        FF_SV_FORMAT(app_name),
        FF_SV_FORMAT(file_name));

    return ff_string_builder_copy_to(&sb, &s_arena);
}

void ff_app_init(ff_string_view name, ff_string_view window_title)
{
    FF_ASSERT_RET(!s_valid && name.count);

    s_valid = true;
    ff_arena_init_heap_global(&s_arena, 0);

    const ff_string_view log_path = user_file_path(name, FF_SVL("log.txt"));
    const ff_string_view settings_path = user_file_path(name, FF_SVL("settings.bin"));

    s_has_log_stream = log_path.count && ff_stream_init_write_file(&s_log_stream, log_path);
    if (s_has_log_stream)
    {
        ff_stream_write(&s_log_stream, ff_string_view_span(FF_SVL("\xEF\xBB\xBF")));
        s_old_log_sink = ff_log_set_sink((ff_log_sink_data){ .sink = log_sink, .cookie = &s_log_stream });
    }

    ff_log_write(ff_log_type_debug, FF_SVL("App init: %.*s\r\n- Log path: %.*s\r\n- Settings path: %.*s"), FF_SV_FORMAT(name), FF_SV_FORMAT(log_path), FF_SV_FORMAT(settings_path));
    ff_settings_init(settings_path);
    ff_dispatch_init(&s_main_dispatch, ff_dispatch_type_main);
    ff_task_init();

    if (window_title.count)
    {
        ff_window_main_init(&s_main_window, window_title);
    }
}

void ff_app_destroy(void)
{
    FF_CHECK_RET(s_valid);

    ff_log_write(ff_log_type_debug, FF_SVL("App destroy"));

    if (s_main_window.hwnd)
    {
        DestroyWindow(s_main_window.hwnd);
        ff_window_handle_messages();
    }

    ff_task_destroy();
    ff_dispatch_flush(&s_main_dispatch);
    ff_settings_save();
    ff_dispatch_destroy(&s_main_dispatch);
    ff_settings_destroy();

    if (s_has_log_stream)
    {
        ff_log_set_sink(s_old_log_sink);
        ff_stream_destroy(&s_log_stream);
        s_old_log_sink = (ff_log_sink_data){ 0 };
        s_has_log_stream = false;
    }

    ff_arena_destroy(&s_arena);
    s_valid = false;
}
