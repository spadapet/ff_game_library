#include "pch.h"
#include "test_app.h"

// sprite_perf still needs the palette half of m7d plus a way to take a sprite count, so it fails
// in load, before a device is created, rather than crashing deeper in.
static const ff_string_view s_needs =
    FF_SVL_INIT("m7d sprite_perf (palette types and a count argument)");

static bool sprite_perf_load(ff_test_app* app)
{
    return ff_test_mode_unavailable(ff_test_mode_sprite_perf.name, s_needs);
}

static bool sprite_perf_render(ff_test_app* app, ff_dx12_commands* commands)
{
    return false;
}

// The old console test grew the sprite count from the keyboard, which needs an input layer that
// ff.base.c does not have yet. A command line count keeps this usable as a non-interactive
// benchmark in the meantime.
const ff_test_mode ff_test_mode_sprite_perf =
{
    .name = FF_SVL_INIT("sprite_perf"),
    .description = FF_SVL_INIT("Stress test sprite throughput, up to hundreds of thousands"),
    .load = sprite_perf_load,
    .render = sprite_perf_render,
};
