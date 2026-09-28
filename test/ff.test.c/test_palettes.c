#include "pch.h"
#include "test_app.h"

#define PALETTES_WORLD_WIDTH 1920.0f
#define PALETTES_WORLD_HEIGHT 1080.0f
#define PALETTES_TEXTURE_SIZE 64
#define PALETTES_SCENE_SIZE 512
#define PALETTES_ROW_COUNT 32
#define PALETTES_RAMP_COUNT 24
#define PALETTES_PI 3.14159265f

static struct
{
    ff_dx12_draw_state draw_state;
    ff_dx12_draw_device draw_device;

    ff_dx12_texture texture;
    ff_dx12_texture_view view;

    // An RGBA texture for the "sprite drawn into a palette target" path, where the index comes
    // from the vertex color and the texture only supplies coverage.
    ff_dx12_texture rgba_texture;
    ff_dx12_texture_view rgba_view;

    // Geometry has no palette-lookup pixel shader; it reaches palettes by writing indexes into an
    // R8_UINT target, which is then displayed as a palette sprite. That offscreen target is this.
    ff_dx12_texture scene_texture;
    ff_dx12_texture_view scene_view;
    ff_dx12_target_texture scene_target;

    ff_dx12_palette_data palette_data;
    ff_dx12_palette_remap swap_remap;
    bool loaded;
} s_palettes;

static uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

// Index 0 is transparent to the shader, so the art starts at 1. A band of indexes rather than a
// single value is what makes palette cycling visible.
static void fill_indexes(uint8_t* pixels)
{
    const float half = (float)PALETTES_TEXTURE_SIZE * 0.5f;

    for (size_t y = 0; y < PALETTES_TEXTURE_SIZE; y++)
    {
        for (size_t x = 0; x < PALETTES_TEXTURE_SIZE; x++)
        {
            const float dx = (float)x - half;
            const float dy = (float)y - half;
            const float distance = sqrtf(dx * dx + dy * dy);

            uint8_t index = 0;

            if (distance < half)
            {
                const uint32_t band = (uint32_t)(distance * (float)PALETTES_RAMP_COUNT / half);
                index = (uint8_t)(1 + (band % PALETTES_RAMP_COUNT));
            }

            pixels[y * PALETTES_TEXTURE_SIZE + x] = index;
        }
    }
}

// Every row holds the same ramp rotated by one step, so advancing the current row animates the
// colors without touching a single pixel of any index texture.
static void fill_palette(uint32_t* colors)
{
    for (size_t row = 0; row < PALETTES_ROW_COUNT; row++)
    {
        uint32_t* entries = colors + row * FF_PALETTE_SIZE;

        for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
        {
            entries[i] = rgba(0, 0, 0, 255);
        }

        entries[0] = 0;

        for (size_t i = 0; i < PALETTES_RAMP_COUNT; i++)
        {
            const float t = (float)((i + row) % PALETTES_RAMP_COUNT) / (float)PALETTES_RAMP_COUNT;
            const float angle = t * 2.0f * PALETTES_PI;

            const uint32_t r = (uint32_t)(127.5f + 127.0f * sinf(angle));
            const uint32_t g = (uint32_t)(127.5f + 127.0f * sinf(angle + 2.09f));
            const uint32_t b = (uint32_t)(127.5f + 127.0f * sinf(angle + 4.19f));

            entries[1 + i] = rgba(r, g, b, 255);
        }
    }
}

// Coverage art for the RGBA-into-palette-target path. The shader takes the index from the vertex
// color when red is below 1, so these texels only need a nonzero alpha where the glyph is solid.
static void fill_coverage(uint32_t* pixels)
{
    const float half = (float)PALETTES_TEXTURE_SIZE * 0.5f;

    for (size_t y = 0; y < PALETTES_TEXTURE_SIZE; y++)
    {
        for (size_t x = 0; x < PALETTES_TEXTURE_SIZE; x++)
        {
            const float dx = fabsf((float)x - half);
            const float dy = fabsf((float)y - half);
            const bool inside = dx + dy < half;

            pixels[y * PALETTES_TEXTURE_SIZE + x] = inside ? rgba(255, 255, 255, 255) : 0;
        }
    }
}

// Maps each ramp index to its opposite, so the same art drawn with this remap pushed is visibly
// inverted while sharing one texture and one palette.
static ff_dx12_palette_remap make_swap_remap(void)
{
    uint8_t bytes[FF_PALETTE_SIZE];

    for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
    {
        bytes[i] = (uint8_t)i;
    }

    for (size_t i = 0; i < PALETTES_RAMP_COUNT; i++)
    {
        bytes[1 + i] = (uint8_t)(1 + (PALETTES_RAMP_COUNT - 1 - i));
    }

    ff_span span;
    span.data = bytes;
    span.size = FF_PALETTE_SIZE;

    return ff_dx12_palette_remap_make(span);
}

static void palettes_destroy(ff_test_app* app)
{
    ff_dx12_palette_data_destroy(&s_palettes.palette_data);
    ff_dx12_target_texture_destroy(&s_palettes.scene_target);
    ff_dx12_texture_view_destroy(&s_palettes.scene_view);
    ff_dx12_texture_destroy(&s_palettes.scene_texture);
    ff_dx12_texture_view_destroy(&s_palettes.rgba_view);
    ff_dx12_texture_destroy(&s_palettes.rgba_texture);
    ff_dx12_texture_view_destroy(&s_palettes.view);
    ff_dx12_texture_destroy(&s_palettes.texture);
    ff_dx12_draw_device_destroy(&s_palettes.draw_device);
    ff_dx12_draw_state_destroy(&s_palettes.draw_state);
}

static bool palettes_init(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_draw_state_init(&s_palettes.draw_state), false);

    if (!ff_dx12_draw_device_init(&s_palettes.draw_device, &s_palettes.draw_state))
    {
        ff_dx12_draw_state_destroy(&s_palettes.draw_state);
        return false;
    }

    // Both index textures are R8_UINT so the shader's Load returns the index unscaled.
    ff_dx12_texture_params params = ff_dx12_texture_params_default(
        PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE);
    params.format = DXGI_FORMAT_R8_UINT;

    ff_dx12_texture_params scene_params = ff_dx12_texture_params_default(
        PALETTES_SCENE_SIZE, PALETTES_SCENE_SIZE);
    scene_params.format = DXGI_FORMAT_R8_UINT;

    ff_dx12_texture_params rgba_params = ff_dx12_texture_params_default(
        PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE);

    if (!ff_dx12_texture_init(&s_palettes.texture, &params) ||
        !ff_dx12_texture_view_init(&s_palettes.view, &s_palettes.texture, 0, 1, 0, 1) ||
        !ff_dx12_texture_init(&s_palettes.rgba_texture, &rgba_params) ||
        !ff_dx12_texture_view_init(&s_palettes.rgba_view, &s_palettes.rgba_texture, 0, 1, 0, 1) ||
        !ff_dx12_texture_init(&s_palettes.scene_texture, &scene_params) ||
        !ff_dx12_texture_view_init(&s_palettes.scene_view, &s_palettes.scene_texture, 0, 1, 0, 1) ||
        !ff_dx12_target_texture_init(&s_palettes.scene_target, &s_palettes.scene_texture, 0, 0, 0))
    {
        palettes_destroy(app);
        return false;
    }

    s_palettes.swap_remap = make_swap_remap();

    // Both the index pixels and the palette rows need a command list, which only exists in render.
    s_palettes.loaded = false;

    return true;
}

static ff_dx12_sprite palette_sprite(ff_dx12_texture_view* view, float size)
{
    ff_dx12_sprite sprite;
    sprite.view = view;
    sprite.world = ff_rect_float_make(-size, -size, size, size);
    sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

    return sprite;
}

static ff_dx12_draw_endpoint palette_endpoint(float x, float y, int32_t index, float size)
{
    ff_dx12_draw_endpoint point;
    point.pos.x = x;
    point.pos.y = y;
    point.color = ff_color_palette(index, 1.0f);
    point.size = size;

    return point;
}

static int32_t ramp_index(size_t step)
{
    return (int32_t)(1 + (step % PALETTES_RAMP_COUNT));
}

// Pass 1. Every geometry draw type writes palette indexes into the R8_UINT scene target. Nothing
// here picks a color: the index is the output, and the palette that resolves it is chosen later
// when this target is drawn as a palette sprite.
static void draw_geometry_scene(ff_dx12_draw_device* device, float phase, size_t frame)
{
    const float center = (float)PALETTES_SCENE_SIZE * 0.5f;

    ff_dx12_draw_device_draw_rectangle(device,
        ff_rect_float_make(24.0f, 24.0f, 488.0f, 488.0f),
        ff_color_palette(ramp_index(frame / 4), 1.0f), 10.0f);

    for (size_t i = 0; i < 5; i++)
    {
        const float inset = 60.0f + (float)i * 18.0f;

        ff_dx12_draw_device_draw_rectangle(device,
            ff_rect_float_make(inset, inset, 200.0f - inset * 0.2f, 200.0f - inset * 0.2f),
            ff_color_palette(ramp_index(frame / 4 + i * 3), 1.0f), 0.0f);
    }

    for (size_t i = 0; i < 6; i++)
    {
        ff_dx12_draw_endpoint circle = palette_endpoint(
            370.0f, 120.0f, ramp_index(frame / 4 + i * 4), 0.0f);
        circle.size = 66.0f - (float)i * 10.0f;

        ff_dx12_draw_device_draw_circle(device, circle, 6.0f, ff_color_none_palette());
    }

    // A polyline shares one depth across its segments, so the whole spiral is a single draw.
    ff_dx12_draw_endpoint spiral[64];

    for (size_t i = 0; i < 64; i++)
    {
        const float t = (float)i / 63.0f;
        const float angle = phase * 0.7f + t * 6.0f * PALETTES_PI;
        const float radius = 20.0f + t * 110.0f;

        spiral[i] = palette_endpoint(
            center - 120.0f + radius * cosf(angle),
            center + 150.0f + radius * sinf(angle),
            ramp_index(frame / 4 + i / 4), 7.0f);
    }

    ff_dx12_draw_device_draw_lines(device, spiral, 64);

    ff_dx12_draw_endpoint fan[3 * 8];

    for (size_t i = 0; i < 8; i++)
    {
        const float a0 = phase * 0.4f + (float)i * 2.0f * PALETTES_PI / 8.0f;
        const float a1 = a0 + 2.0f * PALETTES_PI / 10.0f;
        const int32_t index = ramp_index(frame / 4 + i * 3);

        fan[i * 3 + 0] = palette_endpoint(340.0f, 330.0f, index, 0.0f);
        fan[i * 3 + 1] = palette_endpoint(
            340.0f + 95.0f * cosf(a0), 330.0f + 95.0f * sinf(a0), index, 0.0f);
        fan[i * 3 + 2] = palette_endpoint(
            340.0f + 95.0f * cosf(a1), 330.0f + 95.0f * sinf(a1), index, 0.0f);
    }

    ff_dx12_draw_device_draw_triangles(device, fan, 3 * 8);

    // A palette sprite drawn into a palette target skips the color lookup entirely and writes its
    // remapped index straight out, so index art composites into the scene without a palette.
    const ff_dx12_sprite sprite = palette_sprite(&s_palettes.view, 40.0f);

    for (size_t i = 0; i < 3; i++)
    {
        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 110.0f + (float)i * 95.0f;
        transform.position.y = 440.0f;
        transform.rotation_radians = phase + (float)i * 0.5f;

        ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);
    }

    // An RGBA sprite drawn into a palette target takes its index from the vertex color instead of
    // from the texture, which is the path text rendering uses.
    const ff_dx12_sprite rgba_sprite = palette_sprite(&s_palettes.rgba_view, 40.0f);

    for (size_t i = 0; i < 3; i++)
    {
        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 110.0f + (float)i * 95.0f;
        transform.position.y = 330.0f;
        transform.color = ff_color_palette(ramp_index(frame / 4 + i * 7), 1.0f);

        ff_dx12_draw_device_draw_sprite(device, &rgba_sprite, &transform);
    }
}

// Each sprite sits on a different palette row, so one index texture produces a row of differently
// colored art and every distinct row interns to its own slot of the shared palette texture.
static void draw_palette_row(ff_dx12_draw_device* device, size_t frame)
{
    for (size_t i = 0; i < 5; i++)
    {
        const size_t row = (frame / 2 + i * 5) % PALETTES_ROW_COUNT;

        ff_dx12_draw_device_push_palette(device,
            ff_dx12_palette_make(&s_palettes.palette_data, row));

        const ff_dx12_sprite sprite = palette_sprite(&s_palettes.view, 78.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 1230.0f;
        transform.position.y = 170.0f + (float)i * 180.0f;

        ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);

        ff_dx12_draw_device_pop_palette(device);
    }
}

// The same art and the same palette row, drawn with and without the swap remap, so a broken remap
// shows up as two identical sprites instead of two inverted ones.
static void draw_remap_pair(ff_dx12_draw_device* device, size_t frame)
{
    const size_t row = (frame / 2) % PALETTES_ROW_COUNT;

    ff_dx12_draw_device_push_palette(device,
        ff_dx12_palette_make(&s_palettes.palette_data, row));

    const ff_dx12_sprite sprite = palette_sprite(&s_palettes.view, 100.0f);

    ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
    transform.position.x = 1560.0f;
    transform.position.y = 300.0f;
    ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);

    ff_dx12_draw_device_push_palette_remap(device, &s_palettes.swap_remap);

    transform.position.y = 560.0f;
    ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);

    ff_dx12_draw_device_pop_palette_remap(device);
    ff_dx12_draw_device_pop_palette(device);
}

// Many sprites sharing one palette row collapse into a single instanced draw, which is the whole
// point of interning the palette rather than binding one per sprite.
static void draw_swarm(ff_dx12_draw_device* device, float phase, size_t frame)
{
    const size_t row = (frame / 3) % PALETTES_ROW_COUNT;

    ff_dx12_draw_device_push_palette(device,
        ff_dx12_palette_make(&s_palettes.palette_data, row));
    ff_dx12_draw_device_push_no_overlap(device);

    for (size_t i = 0; i < 28; i++)
    {
        const float angle = phase * 0.5f + (float)i * 2.0f * PALETTES_PI / 28.0f;
        const ff_dx12_sprite sprite = palette_sprite(&s_palettes.view, 24.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 1400.0f + 300.0f * cosf(angle);
        transform.position.y = 890.0f + 110.0f * sinf(angle);

        ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);
    }

    ff_dx12_draw_device_pop_no_overlap(device);
    ff_dx12_draw_device_pop_palette(device);
}

// The scene target holds indexes, so the geometry drawn into it is recolored wholesale just by
// choosing which palette row is active when it is displayed.
static void draw_scene_sprite(ff_dx12_draw_device* device, size_t frame)
{
    const size_t row = (frame / 2) % PALETTES_ROW_COUNT;

    ff_dx12_draw_device_push_palette(device,
        ff_dx12_palette_make(&s_palettes.palette_data, row));

    const ff_dx12_sprite sprite = palette_sprite(&s_palettes.scene_view, 430.0f);

    ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
    transform.position.x = 500.0f;
    transform.position.y = 540.0f;

    ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);

    ff_dx12_draw_device_pop_palette(device);
}

static bool palettes_render(ff_test_app* app, ff_dx12_commands* commands)
{
    const size_t width = ff_test_app_width(app);
    const size_t height = ff_test_app_height(app);
    FF_CHECK_RET_VAL(width && height, false);

    if (!s_palettes.loaded)
    {
        static uint8_t pixels[PALETTES_TEXTURE_SIZE * PALETTES_TEXTURE_SIZE];
        static uint32_t coverage[PALETTES_TEXTURE_SIZE * PALETTES_TEXTURE_SIZE];
        static uint32_t colors[PALETTES_ROW_COUNT * FF_PALETTE_SIZE];

        fill_indexes(pixels);
        fill_coverage(coverage);
        fill_palette(colors);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_palettes.texture, commands, 0, 0, 0, 0,
            pixels, PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE), false);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_palettes.rgba_texture, commands, 0, 0, 0, 0,
            coverage, PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE,
            PALETTES_TEXTURE_SIZE * sizeof(uint32_t)), false);

        FF_CHECK_RET_VAL(ff_dx12_palette_data_init(&s_palettes.palette_data, commands,
            colors, PALETTES_ROW_COUNT), false);

        s_palettes.loaded = true;
    }

    const float phase = (float)app->frame / 40.0f;

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

    ff_dx12_target_size scene_size;
    scene_size.pixel_size.x = PALETTES_SCENE_SIZE;
    scene_size.pixel_size.y = PALETTES_SCENE_SIZE;
    scene_size.rotation = ff_dx12_rotation_none;
    scene_size.dpi_scale = 1.0;

    const ff_rect_float scene_rect = ff_rect_float_make(
        0.0f, 0.0f, (float)PALETTES_SCENE_SIZE, (float)PALETTES_SCENE_SIZE);

    // Index 0 is the transparent entry, so clearing to zero leaves the untouched parts of the
    // scene target invisible rather than filled with palette entry 0.
    const float scene_clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ff_dx12_target_texture_clear(&s_palettes.scene_target, commands, scene_clear);

    FF_CHECK_RET_VAL(ff_dx12_draw_device_begin(&s_palettes.draw_device, commands,
        ff_dx12_target_texture_resource(&s_palettes.scene_target),
        ff_dx12_target_texture_view(&s_palettes.scene_target),
        scene_size, ff_dx12_target_texture_format(&s_palettes.scene_target),
        NULL, scene_rect, scene_rect, false), false);

    draw_geometry_scene(&s_palettes.draw_device, phase, app->frame);

    ff_dx12_draw_device_end(&s_palettes.draw_device);

    ff_dx12_target_size target_size;
    target_size.pixel_size.x = width;
    target_size.pixel_size.y = height;
    target_size.rotation = ff_dx12_rotation_none;
    target_size.dpi_scale = 1.0;

    const ff_rect_float view_rect = ff_rect_float_make(0.0f, 0.0f, (float)width, (float)height);
    const ff_rect_float world_rect = ff_rect_float_make(0.0f, 0.0f, PALETTES_WORLD_WIDTH, PALETTES_WORLD_HEIGHT);

    FF_CHECK_RET_VAL(ff_dx12_draw_device_begin(&s_palettes.draw_device, commands,
        ff_dx12_target_window_resource(&app->target), ff_dx12_target_window_view(&app->target),
        target_size, ff_dx12_target_window_format(), NULL, view_rect, world_rect, false), false);

    draw_scene_sprite(&s_palettes.draw_device, app->frame);
    draw_palette_row(&s_palettes.draw_device, app->frame);
    draw_remap_pair(&s_palettes.draw_device, app->frame);
    draw_swarm(&s_palettes.draw_device, phase, app->frame);

    ff_dx12_draw_device_end(&s_palettes.draw_device);

    ff_dx12_commands_end_event(commands);

    return true;
}

const ff_test_mode ff_test_mode_palettes =
{
    .name = FF_SVL_INIT("palettes"),
    .description = FF_SVL_INIT("Palette lines, shapes and sprites with row cycling and remaps"),
    .init = palettes_init,
    .destroy = palettes_destroy,
    .render = palettes_render,
};
