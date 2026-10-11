#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_device_child.h"
#include "dx12_matrix.h"
#include "dx12_object_cache.h"
#include "dx12_shader.h"

typedef struct ff_dx12_commands ff_dx12_commands;

#define FF_DX12_MAX_TEXTURES 32
#define FF_DX12_MAX_PALETTE_TEXTURES 32
#define FF_DX12_MAX_PALETTES 128
#define FF_DX12_MAX_PALETTE_REMAPS 128
#define FF_DX12_MAX_TRANSFORM_MATRIXES 128

// Root parameter slots, in the order the root signature declares them.
typedef enum ff_dx12_root_param
{
    ff_dx12_root_param_vs_constants_0,
    ff_dx12_root_param_vs_constants_1,
    ff_dx12_root_param_ps_constants_0,
    ff_dx12_root_param_samplers,
    ff_dx12_root_param_textures,
    ff_dx12_root_param_palette_textures,
    ff_dx12_root_param_palettes,
    ff_dx12_root_param_count,
} ff_dx12_root_param;

typedef enum ff_dx12_sampler
{
    ff_dx12_sampler_point,
    ff_dx12_sampler_linear,
    ff_dx12_sampler_count,
} ff_dx12_sampler;

typedef enum ff_dx12_draw_bucket
{
    ff_dx12_draw_bucket_sprites,
    ff_dx12_draw_bucket_palette_sprites,
    ff_dx12_draw_bucket_lines,
    ff_dx12_draw_bucket_triangles,
    ff_dx12_draw_bucket_rectangles_filled,
    ff_dx12_draw_bucket_rectangles_outline,
    ff_dx12_draw_bucket_circles_filled,
    ff_dx12_draw_bucket_circles_outline,
    ff_dx12_draw_bucket_count,
} ff_dx12_draw_bucket;

typedef enum ff_dx12_draw_state_flags
{
    ff_dx12_draw_state_blend_opaque = 0x00,
    ff_dx12_draw_state_blend_alpha = 0x01,
    ff_dx12_draw_state_blend_pma = 0x02,
    ff_dx12_draw_state_blend_mask = 0x03,

    ff_dx12_draw_state_depth_enabled = 0x04,

    ff_dx12_draw_state_target_default = 0x00,
    ff_dx12_draw_state_target_bgra = 0x08,
    ff_dx12_draw_state_target_palette = 0x10,
    ff_dx12_draw_state_target_mask = 0x18,

    ff_dx12_draw_state_count = 0x20,
} ff_dx12_draw_state_flags;

typedef struct ff_dx12_vs_constants_0
{
    ff_matrix projection;
    ff_point_float view_scale;
    float padding[2];
} ff_dx12_vs_constants_0;

typedef struct ff_dx12_vs_constants_1
{
    ff_matrix model[FF_DX12_MAX_TRANSFORM_MATRIXES];
} ff_dx12_vs_constants_1;

typedef struct ff_dx12_ps_constants_0
{
    ff_rect_float texture_palette_sizes[FF_DX12_MAX_PALETTE_TEXTURES];
} ff_dx12_ps_constants_0;

typedef struct ff_dx12_draw_bucket_state
{
    const D3D12_INPUT_ELEMENT_DESC* elements;
    size_t element_count;
    ff_dx12_shader vs;
    ff_dx12_shader ps;
    ff_dx12_shader ps_palette_out;
    ID3D12PipelineState* pipeline_states[ff_dx12_draw_state_count];
} ff_dx12_draw_bucket_state;

typedef struct ff_dx12_draw_state
{
    ff_dx12_device_child device_child;
    ff_dx12_object_cache cache;
    ID3D12RootSignature* root_signature;
    ff_dx12_descriptor_range samplers_gpu;
    ff_dx12_draw_bucket_state buckets[ff_dx12_draw_bucket_count];
} ff_dx12_draw_state;

bool ff_dx12_draw_state_init(ff_dx12_draw_state* state);
void ff_dx12_draw_state_destroy(ff_dx12_draw_state* state);
bool ff_dx12_draw_state_valid(const ff_dx12_draw_state* state);

ID3D12RootSignature* ff_dx12_draw_state_root_signature(ff_dx12_draw_state* state);
ff_dx12_descriptor_range ff_dx12_draw_state_samplers(const ff_dx12_draw_state* state);
bool ff_dx12_draw_state_target_format_valid(DXGI_FORMAT format);
ff_dx12_draw_state_flags ff_dx12_draw_state_make_flags(DXGI_FORMAT target_format, bool has_depth, bool transparent, bool pre_multiplied_alpha);
bool ff_dx12_draw_state_bind(ff_dx12_draw_state* state, ff_dx12_commands* commands);
bool ff_dx12_draw_state_apply(ff_dx12_draw_state* state, ff_dx12_commands* commands, ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags);
bool ff_dx12_draw_state_flags_valid(ff_dx12_draw_state_flags flags);
