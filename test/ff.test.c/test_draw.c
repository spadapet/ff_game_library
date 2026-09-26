#include "pch.h"
#include "test_app.h"

// These three modes all need the same two things, which do not exist yet: the root signature and
// pipeline state permutations from m6-draw-state, and the batched instance buckets from
// dx12-draw-device. Until then ff_dx12_commands_draw cannot be called at all, because there is no
// pipeline state to bind. Each mode fails in load, before a device is created, so the failure is
// one clear message rather than a crash deeper in.
static const ff_string_view s_needs =
    FF_SVL_INIT("m6-draw-state (root signature and pipeline states) and dx12-draw-device (batched instance buckets)");

static bool shapes_load(ff_test_app* app)
{
    return ff_test_mode_unavailable(ff_test_mode_shapes.name, s_needs);
}

static bool shapes_render(ff_test_app* app, ff_dx12_commands* commands)
{
    return false;
}

// Filled and outlined variants of every primitive, plus overlapping alpha to exercise the
// depth-sorted transparent pass.
const ff_test_mode ff_test_mode_shapes =
{
    .name = FF_SVL_INIT("shapes"),
    .description = FF_SVL_INIT("Rectangles, lines, circles and triangles, filled and outlined"),
    .load = shapes_load,
    .render = shapes_render,
};

static bool sprites_load(ff_test_app* app)
{
    return ff_test_mode_unavailable(ff_test_mode_sprites.name, s_needs);
}

static bool sprites_render(ff_test_app* app, ff_dx12_commands* commands)
{
    return false;
}

const ff_test_mode ff_test_mode_sprites =
{
    .name = FF_SVL_INIT("sprites"),
    .description = FF_SVL_INIT("Textured sprites with transforms, tinting and palette lookups"),
    .load = sprites_load,
    .render = sprites_render,
};

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
