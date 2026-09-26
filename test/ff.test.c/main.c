#include "pch.h"
#include "test_app.h"

static const ff_string_view s_app_name = FF_SVL_INIT("ff.test.c");

static const ff_test_mode* const s_modes[] =
{
    &ff_test_mode_blit,
    &ff_test_mode_shapes,
    &ff_test_mode_sprites,
    &ff_test_mode_sprite_perf,
};

static void console_log_sink(ff_log_type type, ff_string_view text, void* cookie)
{
    fwrite(text.data, 1, text.count, stdout);
    fflush(stdout);
}

static const ff_test_mode* find_mode(ff_string_view name)
{
    for (size_t i = 0; i < _countof(s_modes); i++)
    {
        if (ff_string_equal(s_modes[i]->name, name))
        {
            return s_modes[i];
        }
    }

    return NULL;
}

static void print_usage(void)
{
    ff_log_write(ff_log_type_debug, FF_SVL("Usage: ff.test.c [mode] [seconds]"));
    ff_log_write(ff_log_type_debug, FF_SVL("Modes:"));

    for (size_t i = 0; i < _countof(s_modes); i++)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("  %-12.*s %.*s"),
            (int)s_modes[i]->name.count, s_modes[i]->name.data,
            (int)s_modes[i]->description.count, s_modes[i]->description.data);
    }

    ff_log_write(ff_log_type_debug,
        FF_SVL("Seconds is optional; a positive value exits with a summary instead of running until closed."));
}

// Choosing the mode from the command line rather than a stdin menu keeps the benchmark path
// non-interactive, which is what makes it usable for catching frame pacing regressions. A menu
// can come back once there is an input layer to drive it.
static const ff_test_mode* choose_mode(int argc, char** argv)
{
    if (argc <= 1)
    {
        return s_modes[0];
    }

    const ff_string_view name = ff_sz_view(argv[1]);

    if (ff_string_equal(name, FF_SVL("-?")) ||
        ff_string_equal(name, FF_SVL("-h")) ||
        ff_string_equal(name, FF_SVL("--help")))
    {
        print_usage();
        return NULL;
    }

    const ff_test_mode* mode = find_mode(name);

    if (!mode)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("Unknown mode: %.*s"), (int)name.count, name.data);
        print_usage();
    }

    return mode;
}

int main(int argc, char** argv)
{
    ff_app_init(s_app_name, s_app_name);

    ff_log_sink_data log_sink;
    log_sink.sink = console_log_sink;
    log_sink.cookie = NULL;
    ff_log_set_sink(log_sink);

    // Debug logging is off by default in Release, but the benchmark output is the entire point of
    // this sample, so turn it on regardless of configuration.
    ff_log_set_type_enabled(ff_log_type_debug, true);

    const ff_test_mode* mode = choose_mode(argc, argv);

    if (!mode)
    {
        ff_app_destroy();
        return 1;
    }

    // Optional "seconds to run" argument, so the sample can be used as a non-interactive
    // benchmark that exits on its own with a summary.
    const double run_seconds = (argc > 2) ? atof(argv[2]) : 0.0;

    const int result = ff_test_app_run(mode, run_seconds);

    ff_app_destroy();

    return result;
}
