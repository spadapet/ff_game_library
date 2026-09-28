#include "pch.h"
#include "test_app.h"

#define PERF_WORLD_WIDTH 1920.0f
#define PERF_WORLD_HEIGHT 1080.0f
#define PERF_TEXTURE_SIZE 64
#define PERF_ATLAS_CELLS 2
#define PERF_PALETTE_ROWS 4
#define PERF_DEFAULT_SPRITES 20000
#define PERF_MAX_SPRITES 1000000
#define PERF_PI 3.14159265f

// Each sprite gets its own phase, radius and cell so the batch isn't one instance repeated. A
// sprite's transform is derived from these every frame rather than stored, which keeps the
// per-sprite footprint at 16 bytes and the working set inside cache even at a million sprites.
typedef struct perf_sprite
{
    float angle;
    float radius;
    float speed;
    uint32_t cell;
} perf_sprite;

typedef enum perf_kind
{
    perf_kind_sprite,
    perf_kind_palette_sprite,
    perf_kind_count,
} perf_kind;

static struct
{
    ff_dx12_draw_state draw_state;
    ff_dx12_draw_device draw_device;

    ff_dx12_texture texture;
    ff_dx12_texture_view view;
    ff_dx12_texture palette_texture;
    ff_dx12_texture_view palette_view;
    ff_dx12_palette_data palette_data;
    bool textures_filled;

    ff_arena arena;
    perf_sprite* sprites;
    size_t capacity;
    size_t count;

    perf_kind kind;
    bool no_overlap;
    bool translucent;
    bool paused;

    // Measured around the draw calls only, so the cost of building the batch is separable from
    // the frame time, which is dominated by waiting for vsync.
    double build_ms;
} s_perf;

static uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

static void fill_atlas(uint32_t* pixels)
{
    const size_t cell = PERF_TEXTURE_SIZE / PERF_ATLAS_CELLS;
    const float half = (float)cell * 0.5f;

    for (size_t y = 0; y < PERF_TEXTURE_SIZE; y++)
    {
        for (size_t x = 0; x < PERF_TEXTURE_SIZE; x++)
        {
            const size_t cell_x = x / cell;
            const size_t cell_y = y / cell;
            const size_t in_x = x % cell;
            const size_t in_y = y % cell;

            const float dx = (float)in_x - half;
            const float dy = (float)in_y - half;
            const float distance = sqrtf(dx * dx + dy * dy);

            uint32_t color;

            if (cell_x == 0 && cell_y == 0)
            {
                color = rgba(80, 160, 255, 255);
            }
            else if (cell_x == 1 && cell_y == 0)
            {
                color = (distance < half) ? rgba(255, 96, 96, 255) : 0;
            }
            else if (cell_x == 0 && cell_y == 1)
            {
                const bool light = (((in_x / 8) + (in_y / 8)) & 1) != 0;
                color = light ? rgba(255, 216, 64, 255) : rgba(64, 32, 0, 255);
            }
            else
            {
                const uint32_t ramp = (uint32_t)((in_x + in_y) * 255 / (cell * 2 - 1));
                color = rgba(ramp, 255 - ramp, 160, 255);
            }

            pixels[y * PERF_TEXTURE_SIZE + x] = color;
        }
    }
}

// The palette-index version of the same atlas. Index 0 is reserved as transparent by the shader,
// so every visible pixel is at least 1.
static void fill_index_atlas(uint8_t* pixels)
{
    const size_t cell = PERF_TEXTURE_SIZE / PERF_ATLAS_CELLS;
    const float half = (float)cell * 0.5f;

    for (size_t y = 0; y < PERF_TEXTURE_SIZE; y++)
    {
        for (size_t x = 0; x < PERF_TEXTURE_SIZE; x++)
        {
            const size_t cell_x = x / cell;
            const size_t cell_y = y / cell;
            const size_t in_x = x % cell;
            const size_t in_y = y % cell;

            const float dx = (float)in_x - half;
            const float dy = (float)in_y - half;
            const float distance = sqrtf(dx * dx + dy * dy);

            uint8_t index;

            if (cell_x == 0 && cell_y == 0)
            {
                index = 1;
            }
            else if (cell_x == 1 && cell_y == 0)
            {
                index = (distance < half) ? 2 : 0;
            }
            else if (cell_x == 0 && cell_y == 1)
            {
                index = (((in_x / 8) + (in_y / 8)) & 1) ? 3 : 4;
            }
            else
            {
                index = (uint8_t)(1 + ((in_x + in_y) * 5) / (cell * 2));
            }

            pixels[y * PERF_TEXTURE_SIZE + x] = index;
        }
    }
}

static void fill_palette(uint32_t* colors)
{
    for (size_t row = 0; row < PERF_PALETTE_ROWS; row++)
    {
        uint32_t* entries = colors + row * FF_PALETTE_SIZE;

        for (size_t i = 0; i < FF_PALETTE_SIZE; i++)
        {
            if (i == 0)
            {
                entries[i] = 0;
                continue;
            }

            // Each row is the same ramp rotated, so switching rows is visibly a recolor rather
            // than a different picture.
            const uint32_t t = (uint32_t)((i + row * 64) % 256);

            entries[i] = rgba(t, 255 - (t * 3) / 5, 90 + (t * 165) / 255, 255);
        }
    }
}

// Deterministic, so two runs at the same count draw the same scene and their timings compare.
static uint32_t next_random(uint32_t* state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static bool reserve_sprites(size_t capacity)
{
    FF_CHECK_RET_VAL(capacity > s_perf.capacity, true);

    size_t new_capacity = s_perf.capacity ? s_perf.capacity : 4096;

    while (new_capacity < capacity)
    {
        new_capacity *= 2;
    }

    if (new_capacity > PERF_MAX_SPRITES)
    {
        new_capacity = PERF_MAX_SPRITES;
    }

    perf_sprite* sprites = ff_arena_realloc_type(&s_perf.arena, perf_sprite,
        s_perf.sprites, s_perf.capacity, new_capacity);

    FF_CHECK_RET_VAL(sprites, false);

    s_perf.sprites = sprites;
    s_perf.capacity = new_capacity;

    return true;
}

static bool set_sprite_count(size_t count)
{
    if (count > PERF_MAX_SPRITES)
    {
        count = PERF_MAX_SPRITES;
    }

    FF_CHECK_RET_VAL(reserve_sprites(count), false);

    // Only the newly added tail is initialized, so growing the count doesn't reshuffle the
    // sprites already on screen and the scene grows rather than changing.
    uint32_t state = (uint32_t)s_perf.count * 2654435761u + 1u;

    for (size_t i = s_perf.count; i < count; i++)
    {
        perf_sprite* sprite = &s_perf.sprites[i];

        sprite->angle = (float)(next_random(&state) % 62832) / 10000.0f;
        sprite->radius = 40.0f + (float)(next_random(&state) % 5000) / 10.0f;
        sprite->speed = 0.15f + (float)(next_random(&state) % 1000) / 1000.0f;
        sprite->cell = next_random(&state) % (PERF_ATLAS_CELLS * PERF_ATLAS_CELLS);
    }

    s_perf.count = count;

    return true;
}

static bool perf_init(ff_test_app* app)
{
    FF_CHECK_RET_VAL(ff_dx12_draw_state_init(&s_perf.draw_state), false);

    if (!ff_dx12_draw_device_init(&s_perf.draw_device, &s_perf.draw_state))
    {
        ff_dx12_draw_state_destroy(&s_perf.draw_state);
        return false;
    }

    ff_dx12_texture_params params = ff_dx12_texture_params_default(
        PERF_TEXTURE_SIZE, PERF_TEXTURE_SIZE);

    ff_dx12_texture_params index_params = ff_dx12_texture_params_default(
        PERF_TEXTURE_SIZE, PERF_TEXTURE_SIZE);

    index_params.format = DXGI_FORMAT_R8_UINT;

    if (!ff_dx12_texture_init(&s_perf.texture, &params) ||
        !ff_dx12_texture_view_init(&s_perf.view, &s_perf.texture, 0, 1, 0, 1) ||
        !ff_dx12_texture_init(&s_perf.palette_texture, &index_params) ||
        !ff_dx12_texture_view_init(&s_perf.palette_view, &s_perf.palette_texture, 0, 1, 0, 1))
    {
        ff_dx12_texture_view_destroy(&s_perf.palette_view);
        ff_dx12_texture_destroy(&s_perf.palette_texture);
        ff_dx12_texture_view_destroy(&s_perf.view);
        ff_dx12_texture_destroy(&s_perf.texture);
        ff_dx12_draw_device_destroy(&s_perf.draw_device);
        ff_dx12_draw_state_destroy(&s_perf.draw_state);
        return false;
    }

    // The palette is created in render alongside the texture uploads, since it needs a command
    // list to get its colors onto the GPU.
    s_perf.textures_filled = false;

    return true;
}

static void perf_destroy(ff_test_app* app)
{
    if (s_perf.textures_filled)
    {
        ff_dx12_palette_data_destroy(&s_perf.palette_data);
    }

    ff_dx12_texture_view_destroy(&s_perf.palette_view);
    ff_dx12_texture_destroy(&s_perf.palette_texture);
    ff_dx12_texture_view_destroy(&s_perf.view);
    ff_dx12_texture_destroy(&s_perf.texture);
    ff_dx12_draw_device_destroy(&s_perf.draw_device);
    ff_dx12_draw_state_destroy(&s_perf.draw_state);
}

static size_t s_start_count = PERF_DEFAULT_SPRITES;

void ff_test_sprite_perf_set_start_count(size_t count)
{
    s_start_count = (count > PERF_MAX_SPRITES) ? PERF_MAX_SPRITES : count;
}

static bool perf_load(ff_test_app* app)
{
    ff_arena_init_heap_global(&s_perf.arena, 64 * 1024);

    s_perf.kind = perf_kind_sprite;
    s_perf.no_overlap = true;

    return set_sprite_count(s_start_count);
}

static void perf_unload(ff_test_app* app)
{
    ff_arena_destroy(&s_perf.arena);
    s_perf.sprites = NULL;
    s_perf.capacity = 0;
    s_perf.count = 0;
}

static ff_rect_float atlas_cell(size_t index)
{
    const float step = 1.0f / (float)PERF_ATLAS_CELLS;
    const float left = (float)(index % PERF_ATLAS_CELLS) * step;
    const float top = (float)(index / PERF_ATLAS_CELLS) * step;

    return ff_rect_float_make(left, top, left + step, top + step);
}

static void draw_sprites(ff_dx12_draw_device* device, float phase)
{
    const bool palette = (s_perf.kind == perf_kind_palette_sprite);

    // The whole field is declared non-overlapping by default, which is what lets a hundred
    // thousand sprites collapse into one instanced draw instead of one call each. Toggling it off
    // is the point of the N key: it shows what the depth nudge costs.
    if (s_perf.no_overlap)
    {
        ff_dx12_draw_device_push_no_overlap(device);
    }

    ff_dx12_sprite sprite;
    sprite.view = palette ? &s_perf.palette_view : &s_perf.view;
    sprite.world = ff_rect_float_make(-16.0f, -16.0f, 16.0f, 16.0f);
    sprite.transparent = false;

    ff_dx12_sprite_transform transform = ff_dx12_sprite_transform_default();

    if (s_perf.translucent)
    {
        transform.color = ff_color_rgba(1.0f, 1.0f, 1.0f, 0.6f);
    }

    const float center_x = PERF_WORLD_WIDTH * 0.5f;
    const float center_y = PERF_WORLD_HEIGHT * 0.5f;

    for (size_t i = 0; i < s_perf.count; i++)
    {
        const perf_sprite* item = &s_perf.sprites[i];
        const float angle = item->angle + phase * item->speed;

        transform.position.x = center_x + item->radius * cosf(angle) * 1.7f;
        transform.position.y = center_y + item->radius * sinf(angle);
        transform.rotation_radians = angle;

        sprite.texture_uv = atlas_cell(item->cell);

        if (palette)
        {
            ff_dx12_draw_device_draw_palette_sprite(device, &sprite, &transform);
        }
        else
        {
            ff_dx12_draw_device_draw_sprite(device, &sprite, &transform);
        }
    }

    if (s_perf.no_overlap)
    {
        ff_dx12_draw_device_pop_no_overlap(device);
    }
}

static bool perf_render(ff_test_app* app, ff_dx12_commands* commands)
{
    const size_t width = ff_test_app_width(app);
    const size_t height = ff_test_app_height(app);
    FF_CHECK_RET_VAL(width && height, false);

    if (!s_perf.textures_filled)
    {
        static uint32_t pixels[PERF_TEXTURE_SIZE * PERF_TEXTURE_SIZE];
        fill_atlas(pixels);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_perf.texture, commands, 0, 0, 0, 0,
            pixels, PERF_TEXTURE_SIZE, PERF_TEXTURE_SIZE,
            PERF_TEXTURE_SIZE * sizeof(uint32_t)), false);

        static uint8_t indexes[PERF_TEXTURE_SIZE * PERF_TEXTURE_SIZE];
        fill_index_atlas(indexes);

        FF_CHECK_RET_VAL(ff_dx12_texture_update(&s_perf.palette_texture, commands, 0, 0, 0, 0,
            indexes, PERF_TEXTURE_SIZE, PERF_TEXTURE_SIZE, PERF_TEXTURE_SIZE), false);

        static uint32_t colors[PERF_PALETTE_ROWS * FF_PALETTE_SIZE];
        fill_palette(colors);

        FF_CHECK_RET_VAL(ff_dx12_palette_data_init(&s_perf.palette_data, commands,
            colors, PERF_PALETTE_ROWS), false);

        s_perf.textures_filled = true;
    }

    ff_dx12_target_size target_size;
    target_size.pixel_size.x = width;
    target_size.pixel_size.y = height;
    target_size.rotation = ff_dx12_rotation_none;
    target_size.dpi_scale = 1.0;

    const ff_rect_float view_rect = ff_rect_float_make(0.0f, 0.0f, (float)width, (float)height);
    const ff_rect_float world_rect = ff_rect_float_make(0.0f, 0.0f, PERF_WORLD_WIDTH, PERF_WORLD_HEIGHT);

    FF_CHECK_RET_VAL(ff_dx12_draw_device_begin(&s_perf.draw_device, commands,
        ff_dx12_target_window_resource(&app->target), ff_dx12_target_window_view(&app->target),
        target_size, ff_dx12_target_window_format(), NULL, view_rect, world_rect, false), false);

    static float phase;

    if (!s_perf.paused)
    {
        phase += 1.0f / 60.0f;
    }

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

    ff_dx12_palette palette = ff_dx12_palette_make(&s_perf.palette_data, 0);
    ff_dx12_draw_device_push_palette(&s_perf.draw_device, palette);

    const int64_t build_start = ff_test_perf_now();

    draw_sprites(&s_perf.draw_device, phase);

    // end_draw is where the batch actually turns into draw calls, so it belongs inside the
    // measurement. Timing only the loop would credit the batching for work it merely deferred.
    ff_dx12_draw_device_end(&s_perf.draw_device);

    const int64_t build_end = ff_test_perf_now();

    s_perf.build_ms = (double)(build_end - build_start) * 1000.0 / (double)app->stats.frequency;

    ff_dx12_draw_device_pop_palette(&s_perf.draw_device);

    ff_dx12_commands_end_event(commands);

    return true;
}

static void perf_status(ff_test_app* app, char* text, size_t count)
{
    const double per_sprite_ns = s_perf.count
        ? (s_perf.build_ms * 1000000.0) / (double)s_perf.count
        : 0.0;

    _snprintf_s(text, count, _TRUNCATE,
        "%zu %s%s%s | build %.2f ms (%.1f ns/sprite)",
        s_perf.count,
        (s_perf.kind == perf_kind_palette_sprite) ? "palette" : "rgba",
        s_perf.no_overlap ? "" : " overlap",
        s_perf.translucent ? " alpha" : "",
        s_perf.build_ms, per_sprite_ns);
}

static void perf_key_down(ff_test_app* app, int virtual_key)
{
    switch (virtual_key)
    {
        // Doubling rather than adding a fixed step: finding the count where the frame rate breaks
        // takes a handful of presses instead of hundreds.
        case VK_SPACE:
            set_sprite_count(s_perf.count ? s_perf.count * 2 : 1024);
            break;

        case VK_BACK:
            set_sprite_count(s_perf.count / 2);
            break;

        case VK_DELETE:
            s_perf.count = 0;
            break;

        case 'P':
            s_perf.kind = (s_perf.kind == perf_kind_sprite)
                ? perf_kind_palette_sprite
                : perf_kind_sprite;
            break;

        case 'N':
            s_perf.no_overlap = !s_perf.no_overlap;
            break;

        case 'T':
            s_perf.translucent = !s_perf.translucent;
            break;

        // Freezing the motion without freezing the loop, so the same batch is rebuilt every frame
        // and the timing can be read off a still image.
        case VK_PAUSE:
        case 'S':
            s_perf.paused = !s_perf.paused;
            break;
    }
}

// SPACE doubles the sprite count, so a few presses walk from the default to the point where the
// renderer can no longer hold the refresh rate. That crossover is the number this mode exists to
// find, and the status line reports build time per sprite so it can be compared across counts.
const ff_test_mode ff_test_mode_sprite_perf =
{
    .name = FF_SVL_INIT("sprite_perf"),
    .description = FF_SVL_INIT("Stress sprite throughput: SPACE doubles, BACK halves, DEL clears, P palette, N overlap, T alpha, S pause"),
    .load = perf_load,
    .unload = perf_unload,
    .init = perf_init,
    .destroy = perf_destroy,
    .render = perf_render,
    .status = perf_status,
    .key_down = perf_key_down,
};
