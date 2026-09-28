#include "pch.h"
#include "test_app.h"

#define PALETTES_WORLD_WIDTH 1920.0f
#define PALETTES_WORLD_HEIGHT 1080.0f
#define PALETTES_TEXTURE_SIZE 64
#define PALETTES_ROW_COUNT 32
#define PALETTES_RAMP_COUNT 24
#define PALETTES_PI 3.14159265f

static struct
{
    ff_dx12_draw_state draw_state;
    ff_dx12_draw_device draw_device;
    ff_dx12_texture texture;
    ff_dx12_texture_view view;
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
// colors without touching a single pixel of the index texture.
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

static bool palettes_init(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_draw_state_init(&s_palettes.draw_state), false);

    if (!ff_dx12_draw_device_init(&s_palettes.draw_device, &s_palettes.draw_state))
    {
        ff_dx12_draw_state_destroy(&s_palettes.draw_state);
        return false;
    }

    // The index texture is R8_UINT so the shader's Load returns the index unscaled.
    ff_dx12_texture_params params = ff_dx12_texture_params_default(
        PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE);
    params.format = DXGI_FORMAT_R8_UINT;

    if (!ff_dx12_texture_init(&s_palettes.texture, &params) ||
        !ff_dx12_texture_view_init(&s_palettes.view, &s_palettes.texture, 0, 1, 0, 1))
    {
        ff_dx12_texture_view_destroy(&s_palettes.view);
        ff_dx12_texture_destroy(&s_palettes.texture);
        ff_dx12_draw_device_destroy(&s_palettes.draw_device);
        ff_dx12_draw_state_destroy(&s_palettes.draw_state);
        return false;
    }

    s_palettes.swap_remap = make_swap_remap();

    // Both the index pixels and the palette rows need a command list, which only exists in render.
    s_palettes.loaded = false;

    return true;
}

static void palettes_destroy(ff_test_app* app)
{
    ff_dx12_palette_data_destroy(&s_palettes.palette_data);
    ff_dx12_texture_view_destroy(&s_palettes.view);
    ff_dx12_texture_destroy(&s_palettes.texture);
    ff_dx12_draw_device_destroy(&s_palettes.draw_device);
    ff_dx12_draw_state_destroy(&s_palettes.draw_state);
}

static ff_dx12_sprite palette_sprite(float size)
{
    ff_dx12_sprite sprite;
    sprite.view = &s_palettes.view;
    sprite.world = ff_rect_float_make(-size, -size, size, size);
    sprite.texture_uv = ff_rect_float_make(0.0f, 0.0f, 1.0f, 1.0f);

    return sprite;
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

        const ff_dx12_sprite sprite = palette_sprite(90.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 260.0f + (float)i * 350.0f;
        transform.position.y = 260.0f;

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

    const ff_dx12_sprite sprite = palette_sprite(120.0f);

    ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
    transform.position.x = 700.0f;
    transform.position.y = 640.0f;
    ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);

    ff_dx12_draw_device_push_palette_remap(device, &s_palettes.swap_remap);

    transform.position.x = 1120.0f;
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

    for (size_t i = 0; i < 36; i++)
    {
        const float angle = phase * 0.5f + (float)i * 2.0f * PALETTES_PI / 36.0f;
        const ff_dx12_sprite sprite = palette_sprite(28.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 960.0f + 420.0f * cosf(angle);
        transform.position.y = 880.0f + 130.0f * sinf(angle);

        ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);
    }

    ff_dx12_draw_device_pop_no_overlap(device);
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
        static uint32_t colors[PALETTES_ROW_COUNT * FF_PALETTE_SIZE];

        fill_indexes(pixels);
        fill_palette(colors);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_palettes.texture, commands, 0, 0, 0, 0,
            pixels, PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE, PALETTES_TEXTURE_SIZE), false);

        FF_CHECK_RET_VAL(ff_dx12_palette_data_init(&s_palettes.palette_data, commands,
            colors, PALETTES_ROW_COUNT), false);

        s_palettes.loaded = true;
    }

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

    const float phase = (float)app->frame / 40.0f;

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

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
    .description = FF_SVL_INIT("Palette sprites with row cycling and index remaps"),
    .init = palettes_init,
    .destroy = palettes_destroy,
    .render = palettes_render,
};
