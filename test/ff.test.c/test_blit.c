#include "pch.h"
#include "test_app.h"

static const ff_string_view s_sprite_file = FF_SVL_INIT("assets\\sprite.png");

static struct
{
    ff_dx12_texture sprite;
    ff_arena sprite_arena;
    uint32_t* sprite_pixels;
    size_t sprite_width;
    size_t sprite_height;
} s_blit;

// The swap chain is BGRA and the decoder always produces RGBA, so red and blue are exchanged
// here rather than asking for a second texture format. A miss shows up immediately on screen.
static void swizzle_rgba_to_bgra(uint32_t* pixels, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const uint32_t value = pixels[i];
        pixels[i] = (value & 0xFF00FF00u) | ((value & 0x00FF0000u) >> 16) | ((value & 0x000000FFu) << 16);
    }
}

static bool blit_load(ff_test_app* app)
{
    ff_arena_init_heap_local(&s_blit.sprite_arena, 0);

    ff_arena_declare_stack(path_arena, 1024);
    const ff_string_view path = ff_test_asset_path(s_sprite_file, &path_arena);

    ff_file_map map;
    const bool mapped = path.count && ff_file_map_init(&map, path);

    if (!mapped)
    {
        ff_log_write(ff_log_type_debug, FF_SVL("Can't open sprite: %.*s"), (int)path.count, path.data);
    }

    ff_arena_destroy(&path_arena);
    FF_CHECK_RET_VAL(mapped, false);

    ff_png_image image;
    const bool decoded = ff_png_decode(ff_file_map_data(&map), &s_blit.sprite_arena, false, &image);
    ff_file_map_destroy(&map);

    FF_CHECK_RET_VAL(decoded, false);

    s_blit.sprite_pixels = (uint32_t*)image.pixels;
    s_blit.sprite_width = image.width;
    s_blit.sprite_height = image.height;

    swizzle_rgba_to_bgra(s_blit.sprite_pixels, (size_t)image.width * (size_t)image.height);

    ff_log_write(ff_log_type_debug, FF_SVL("Loaded sprite: %ux%u"), image.width, image.height);

    return true;
}

static void blit_unload(ff_test_app* app)
{
    ff_arena_destroy(&s_blit.sprite_arena);
    s_blit.sprite_pixels = NULL;
}

static bool blit_init(ff_test_app* app)
{
    ff_dx12_texture_params params = ff_dx12_texture_params_default(s_blit.sprite_width, s_blit.sprite_height);
    params.format = ff_dx12_target_window_format();

    return ff_dx12_texture_init(&s_blit.sprite, &params);
}

static void blit_destroy(ff_test_app* app)
{
    ff_dx12_texture_destroy(&s_blit.sprite);
}

// The decoded pixels only have to be uploaded when the texture is new, since nothing animates
// them any more. A device reset recreates the texture, so this runs again from init.
static bool upload_sprite(ff_dx12_commands* commands)
{
    return ff_dx12_texture_update(&s_blit.sprite, commands, 0, 0, 0, 0,
        s_blit.sprite_pixels, s_blit.sprite_width, s_blit.sprite_height,
        s_blit.sprite_width * sizeof(uint32_t));
}

static bool blit_render(ff_test_app* app, ff_dx12_commands* commands)
{
    const size_t width = ff_test_app_width(app);
    const size_t height = ff_test_app_height(app);
    FF_CHECK_RET_VAL(width >= s_blit.sprite_width && height >= s_blit.sprite_height, false);
    FF_CHECK_RET_VAL(upload_sprite(commands), false);

    const size_t max_x = width - s_blit.sprite_width;
    const size_t max_y = height - s_blit.sprite_height;
    const size_t dest_x = max_x ? (size_t)(app->frame % max_x) : 0;
    const size_t dest_y = max_y ? (size_t)((app->frame / 2) % max_y) : 0;

    const D3D12_RECT source_rect =
    {
        .left = 0,
        .top = 0,
        .right = (LONG)s_blit.sprite_width,
        .bottom = (LONG)s_blit.sprite_height,
    };

    ff_dx12_commands_begin_event(commands, ff_dx12_gpu_event_draw_2d);

    ff_dx12_commands_copy_texture(commands,
        ff_dx12_target_window_resource(&app->target), 0, dest_x, dest_y,
        ff_dx12_texture_resource(&s_blit.sprite), 0, &source_rect);

    ff_dx12_commands_end_event(commands);

    return true;
}

const ff_test_mode ff_test_mode_blit =
{
    .name = FF_SVL_INIT("blit"),
    .description = FF_SVL_INIT("Animate a decoded PNG by copying it into the back buffer"),
    .load = blit_load,
    .unload = blit_unload,
    .init = blit_init,
    .destroy = blit_destroy,
    .render = blit_render,
};
