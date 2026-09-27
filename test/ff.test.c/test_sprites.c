#include "pch.h"
#include "test_app.h"

#define SPRITES_WORLD_WIDTH 1920.0f
#define SPRITES_WORLD_HEIGHT 1080.0f
#define SPRITES_TEXTURE_SIZE 64
#define SPRITES_ATLAS_CELLS 2
#define SPRITES_ORBIT_COUNT 48
#define SPRITES_PI 3.14159265f

static struct
{
    ff_dx12_draw_state draw_state;
    ff_dx12_draw_device draw_device;
    ff_dx12_texture texture;
    ff_dx12_texture_view view;
    bool texture_filled;
} s_sprites;

// The texture is R8G8B8A8_UNORM, so a little-endian uint32_t packs as 0xAABBGGRR. Building the
// value from named components avoids writing that byte order backwards in a literal.
static uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

// A 2x2 atlas: a solid cell, a ring, a checkerboard and a diagonal gradient. Four visually
// distinct cells mean a wrong uv rect shows up as the wrong picture rather than as a subtle shift.
static void fill_atlas(uint32_t* pixels)
{
    const size_t cell = SPRITES_TEXTURE_SIZE / SPRITES_ATLAS_CELLS;
    const float half = (float)cell * 0.5f;

    for (size_t y = 0; y < SPRITES_TEXTURE_SIZE; y++)
    {
        for (size_t x = 0; x < SPRITES_TEXTURE_SIZE; x++)
        {
            const size_t cell_x = x / cell;
            const size_t cell_y = y / cell;
            const size_t in_x = x % cell;
            const size_t in_y = y % cell;

            uint32_t color;

            if (cell_x == 0 && cell_y == 0)
            {
                color = rgba(64, 128, 255, 255);
            }
            else if (cell_x == 1 && cell_y == 0)
            {
                const float dx = (float)in_x - half;
                const float dy = (float)in_y - half;
                const float distance = sqrtf(dx * dx + dy * dy);

                color = (distance < half && distance > half * 0.55f) ? rgba(32, 224, 32, 255) : 0;
            }
            else if (cell_x == 0 && cell_y == 1)
            {
                const bool light = (((in_x / 8) + (in_y / 8)) & 1) != 0;
                color = light ? rgba(255, 255, 255, 255) : rgba(48, 48, 48, 255);
            }
            else
            {
                const uint32_t ramp = (uint32_t)((in_x + in_y) * 255 / (cell * 2 - 1));
                color = rgba(ramp, 128, 255 - ramp, 255);
            }

            pixels[y * SPRITES_TEXTURE_SIZE + x] = color;
        }
    }
}

static bool sprites_init(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_draw_state_init(&s_sprites.draw_state), false);

    if (!ff_dx12_draw_device_init(&s_sprites.draw_device, &s_sprites.draw_state))
    {
        ff_dx12_draw_state_destroy(&s_sprites.draw_state);
        return false;
    }

    ff_dx12_texture_params params = ff_dx12_texture_params_default(
        SPRITES_TEXTURE_SIZE, SPRITES_TEXTURE_SIZE);

    if (!ff_dx12_texture_init(&s_sprites.texture, &params) ||
        !ff_dx12_texture_view_init(&s_sprites.view, &s_sprites.texture, 0, 1, 0, 1))
    {
        ff_dx12_texture_view_destroy(&s_sprites.view);
        ff_dx12_texture_destroy(&s_sprites.texture);
        ff_dx12_draw_device_destroy(&s_sprites.draw_device);
        ff_dx12_draw_state_destroy(&s_sprites.draw_state);
        return false;
    }

    // The pixels have to be uploaded through a command list, which only exists inside render.
    s_sprites.texture_filled = false;

    return true;
}

static void sprites_destroy(ff_test_app* app)
{
    ff_dx12_texture_view_destroy(&s_sprites.view);
    ff_dx12_texture_destroy(&s_sprites.texture);
    ff_dx12_draw_device_destroy(&s_sprites.draw_device);
    ff_dx12_draw_state_destroy(&s_sprites.draw_state);
}

static ff_rect_float atlas_cell(size_t index)
{
    const float step = 1.0f / (float)SPRITES_ATLAS_CELLS;
    const float left = (float)(index % SPRITES_ATLAS_CELLS) * step;
    const float top = (float)(index / SPRITES_ATLAS_CELLS) * step;

    return ff_rect_float_make(left, top, left + step, top + step);
}

static ff_dx12_sprite atlas_sprite(size_t index, float size)
{
    ff_dx12_sprite sprite;
    sprite.view = &s_sprites.view;
    sprite.world = ff_rect_float_make(-size, -size, size, size);
    sprite.texture_uv = atlas_cell(index);

    return sprite;
}

// The world rect is centered on the origin, so rotation happens about each sprite's own center.
static void draw_row(ff_dx12_draw_device* device, float phase)
{
    for (size_t i = 0; i < 4; i++)
    {
        const ff_dx12_sprite sprite = atlas_sprite(i, 80.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 300.0f + (float)i * 280.0f;
        transform.position.y = 220.0f;
        transform.rotation_radians = phase + (float)i * 0.4f;

        ff_dx12_draw_device_draw_sprite(device, &sprite, &transform);
    }
}

static void draw_scaled_and_tinted(ff_dx12_draw_device* device, float phase)
{
    const float pulse = 1.0f + 0.35f * sinf(phase * 1.7f);

    for (size_t i = 0; i < 4; i++)
    {
        const ff_dx12_sprite sprite = atlas_sprite(i, 70.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 300.0f + (float)i * 280.0f;
        transform.position.y = 520.0f;
        transform.scale.x = pulse;
        transform.scale.y = pulse;

        // A tint multiplies the sampled texel, so the atlas art stays recognizable under it.
        const float t = (float)i / 3.0f;
        transform.color = ff_color_rgba(1.0f - t * 0.6f, 0.4f + t * 0.6f, 1.0f, 1.0f);

        ff_dx12_draw_device_draw_sprite(device, &sprite, &transform);
    }
}

// A ring of translucent sprites. They never overlap each other, so the whole ring is declared
// no-overlap and collapses into a single instanced draw call instead of one call per sprite.
static void draw_orbit(ff_dx12_draw_device* device, float phase)
{
    ff_dx12_draw_device_push_no_overlap(device);

    for (size_t i = 0; i < SPRITES_ORBIT_COUNT; i++)
    {
        const float angle = phase * 0.6f + (float)i * 2.0f * SPRITES_PI / (float)SPRITES_ORBIT_COUNT;
        const ff_dx12_sprite sprite = atlas_sprite(1, 26.0f);

        ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();
        transform.position.x = 960.0f + 330.0f * cosf(angle);
        transform.position.y = 820.0f + 170.0f * sinf(angle);
        transform.rotation_radians = -angle;
        transform.color = ff_color_rgba(1.0f, 1.0f, 1.0f, 0.75f);

        ff_dx12_draw_device_draw_sprite(device, &sprite, &transform);
    }

    ff_dx12_draw_device_pop_no_overlap(device);
}

static bool sprites_render(ff_test_app* app, ff_dx12_commands* commands)
{
    const size_t width = ff_test_app_width(app);
    const size_t height = ff_test_app_height(app);
    FF_CHECK_RET_VAL(width && height, false);

    if (!s_sprites.texture_filled)
    {
        static uint32_t pixels[SPRITES_TEXTURE_SIZE * SPRITES_TEXTURE_SIZE];
        fill_atlas(pixels);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_sprites.texture, commands, 0, 0, 0, 0,
            pixels, SPRITES_TEXTURE_SIZE, SPRITES_TEXTURE_SIZE,
            SPRITES_TEXTURE_SIZE * sizeof(uint32_t)), false);

        s_sprites.texture_filled = true;
    }

    ff_dx12_target_size target_size;
    target_size.pixel_size.x = width;
    target_size.pixel_size.y = height;
    target_size.rotation = ff_dx12_rotation_none;
    target_size.dpi_scale = 1.0;

    const ff_rect_float view_rect = ff_rect_float_make(0.0f, 0.0f, (float)width, (float)height);
    const ff_rect_float world_rect = ff_rect_float_make(0.0f, 0.0f, SPRITES_WORLD_WIDTH, SPRITES_WORLD_HEIGHT);

    FF_CHECK_RET_VAL(ff_dx12_draw_device_begin(&s_sprites.draw_device, commands,
        ff_dx12_target_window_resource(&app->target), ff_dx12_target_window_view(&app->target),
        target_size, ff_dx12_target_window_format(), NULL, view_rect, world_rect, false), false);

    const float phase = (float)app->frame / 40.0f;

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

    draw_row(&s_sprites.draw_device, phase);
    draw_scaled_and_tinted(&s_sprites.draw_device, phase);
    draw_orbit(&s_sprites.draw_device, phase);

    ff_dx12_draw_device_end(&s_sprites.draw_device);

    ff_dx12_commands_end_event(commands);

    return true;
}

const ff_test_mode ff_test_mode_sprites =
{
    .name = FF_SVL_INIT("sprites"),
    .description = FF_SVL_INIT("Textured sprites with transforms, tinting and atlas lookups"),
    .init = sprites_init,
    .destroy = sprites_destroy,
    .render = sprites_render,
};
