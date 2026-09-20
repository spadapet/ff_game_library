#include "pch.h"

static void log_sink(ff_log_type type, ff_string_view text, void* cookie)
{
    ff_stream* log_stream = (ff_stream*)cookie;
    ff_stream_write(log_stream, ff_string_view_span(text));
}

static void on_save(void* args, void* cookie)
{
    ff_log_write(ff_log_type_normal, FF_SVL("Settings saved."));
}

int main()
{
    ff_arena arena;
    ff_arena_init_heap_global(&arena, 0);

    ff_string_view app_name = FF_SVL("ff.test.c");
    ff_string_view user_path = ff_file_user_local_path(&arena);

    ff_string_builder sb;
    ff_string_builder_init(&sb, &arena);
    ff_string_builder_append_format(&sb, FF_SVL("%.*s\\%.*s\\log.txt"), FF_SV_FORMAT(user_path), FF_SV_FORMAT(app_name));
    ff_string_view log_path = ff_string_copy(ff_string_builder_view(&sb), &arena);

    ff_string_builder_reset(&sb);
    ff_string_builder_append_format(&sb, FF_SVL("%.*s\\%.*s\\settings.bin"), FF_SV_FORMAT(user_path), FF_SV_FORMAT(app_name));
    ff_string_view settings_path = ff_string_copy(ff_string_builder_view(&sb), &arena);

    ff_log_sink_data old_log_sink = { 0 };
    ff_stream log_stream;
    if (ff_stream_init_write_file(&log_stream, log_path))
    {
        ff_stream_write(&log_stream, ff_string_view_span(FF_SVL("\xEF\xBB\xBF")));
        old_log_sink = ff_log_set_sink((ff_log_sink_data) { .sink = log_sink, .cookie = &log_stream });
    }

    ff_settings_init(settings_path);

    ff_signal_connection save_connection;
    ff_signal_connection_init(&save_connection);
    ff_signal_connect(ff_settings_save_signal(), &save_connection, on_save, NULL);

    ff_dx12_init_params params = ff_dx12_init_params_default();
    if (ff_dx12_init(&params))
    {
        ff_dx12_device_valid();
        ff_dx12_destroy();
    }

    ff_signal_connection_destroy(&save_connection);
    ff_settings_destroy();
    ff_log_set_sink(old_log_sink);
    ff_stream_destroy(&log_stream);
    ff_arena_destroy(&arena);

    return 0;
}
