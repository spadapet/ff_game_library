#pragma once

#include "../base/string.h"

// Compiled shader blobs, loaded by name from .cso files next to the executable.
//
// The entries are (source file, entry point) pairs rather than source files: ps_sprite.hlsl and
// ps_color.hlsl each compile several entry points, and the draw device selects between them from
// its state flags.
typedef enum ff_dx12_shader
{
    ff_dx12_shader_vs_sprite,
    ff_dx12_shader_vs_line,
    ff_dx12_shader_vs_triangle,
    ff_dx12_shader_vs_rectangle,
    ff_dx12_shader_vs_circle,
    ff_dx12_shader_ps_color,
    ff_dx12_shader_ps_color_out_palette,
    ff_dx12_shader_ps_sprite,
    ff_dx12_shader_ps_palette_sprite,
    ff_dx12_shader_ps_sprite_out_palette,
    ff_dx12_shader_ps_palette_sprite_out_palette,
    ff_dx12_shader_count,
} ff_dx12_shader;

// Base name of the .cso file, which is also the HLSL entry point name.
ff_string_view ff_dx12_shader_name(ff_dx12_shader shader);
