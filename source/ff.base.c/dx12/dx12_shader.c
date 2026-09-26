#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_shader.h"

static const ff_string_view s_shader_names[ff_dx12_shader_count] =
{
    FF_SVL_INIT("vs_sprite"),
    FF_SVL_INIT("vs_line"),
    FF_SVL_INIT("vs_triangle"),
    FF_SVL_INIT("vs_rectangle"),
    FF_SVL_INIT("vs_circle"),
    FF_SVL_INIT("ps_color"),
    FF_SVL_INIT("ps_color_out_palette"),
    FF_SVL_INIT("ps_sprite"),
    FF_SVL_INIT("ps_palette_sprite"),
    FF_SVL_INIT("ps_sprite_out_palette"),
    FF_SVL_INIT("ps_palette_sprite_out_palette"),
};

static_assert(sizeof(s_shader_names) / sizeof(s_shader_names[0]) == ff_dx12_shader_count,
    "shader name table must cover every ff_dx12_shader");

ff_string_view ff_dx12_shader_name(ff_dx12_shader shader)
{
    FF_ASSERT_RET_VAL(shader >= 0 && shader < ff_dx12_shader_count, ff_string_view_empty());
    return s_shader_names[shader];
}
