#pragma once

#include "dx12_descriptor_range.h"
#include "dx12_device_child.h"
#include "dx12_matrix.h"
#include "dx12_object_cache.h"
#include "dx12_shader.h"

typedef struct ff_dx12_commands ff_dx12_commands;

// Shader-visible binding limits. These are baked into data.hlsli (textures_[32], palette_textures_
// at t32, palette_ at t64, palette_remap_ at t65), so the root signature ranges and the HLSL
// register assignments have to be changed together.
#define FF_DX12_MAX_TEXTURES 32
#define FF_DX12_MAX_PALETTE_TEXTURES 32
#define FF_DX12_MAX_TRANSFORM_MATRIXES 128
#define FF_DX12_SAMPLER_COUNT 2

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

// Which sampler slot an instance selects out of samplers_[2].
typedef enum ff_dx12_sampler
{
    ff_dx12_sampler_point,
    ff_dx12_sampler_linear,
    ff_dx12_sampler_count,
} ff_dx12_sampler;

// One bucket per (geometry kind, pixel-shader pairing). The order matches the instance bucket
// order the draw device will iterate, so a bucket index maps straight to a state.
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

// PSO permutation key. Blend is a two-bit field rather than independent flags because opaque,
// straight alpha and pre-multiplied alpha are mutually exclusive; the same is true of the target
// format field. Kept as a bit set so the permutations can index a flat array.
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

// Root-constant block for the vertex shader (b0). DWORD_COUNT deliberately excludes the trailing
// padding: the padding exists so the struct matches the HLSL cbuffer's float2 alignment, but the
// root signature must not declare constants that no shader reads.
#define FF_DX12_VS_CONSTANTS_0_DWORD_COUNT 18

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

// Cached pipeline states for one bucket. The state layer owns nothing the device owns: the
// pipelines and the root signature live in the object cache, so these are borrowed pointers that
// go stale on a device reset and are simply dropped rather than released.
typedef struct ff_dx12_draw_bucket_state
{
    const D3D12_INPUT_ELEMENT_DESC* elements;
    size_t element_count;
    ff_dx12_shader vs;
    ff_dx12_shader ps;
    ff_dx12_shader ps_palette_out;
    ID3D12PipelineState* pipeline_states[ff_dx12_draw_state_count];
} ff_dx12_draw_bucket_state;

// The state half of the draw device: root signature, samplers, and the PSO permutation matrix.
// Split from the draw device itself so the batching layer can be built and tested on top of a
// state layer that is already known good.
//
// The object cache is owned here rather than shared globally so that its lifetime is strictly
// contained by the state that borrows from it: every pipeline and root signature pointer below
// points into this cache, and both die together.
typedef struct ff_dx12_draw_state
{
    ff_dx12_device_child device_child;
    ff_dx12_object_cache cache;
    ID3D12RootSignature* root_signature;
    ff_dx12_descriptor_range samplers_gpu;
    ff_dx12_draw_bucket_state buckets[ff_dx12_draw_bucket_count];
} ff_dx12_draw_state;

bool ff_dx12_draw_state_init(ff_dx12_draw_state* state);

// Releases the root signature and every cached pipeline immediately. Command lists do not hold
// references on the pipelines they were given, so the caller must ensure no in-flight GPU work
// still references this state, the same contract the object cache itself has. In practice the
// draw device owns one of these for its whole lifetime and tears it down at shutdown, after
// ff_dx12_wait_for_idle.
void ff_dx12_draw_state_destroy(ff_dx12_draw_state* state);

bool ff_dx12_draw_state_valid(const ff_dx12_draw_state* state);

// Borrowed; owned by the object cache and invalidated by a device reset.
ID3D12RootSignature* ff_dx12_draw_state_root_signature(ff_dx12_draw_state* state);
ff_dx12_descriptor_range ff_dx12_draw_state_samplers(const ff_dx12_draw_state* state);

// True only for the formats the pixel shaders can actually write.
bool ff_dx12_draw_state_target_format_valid(DXGI_FORMAT format);

// Folds the draw-time conditions into a permutation index. Blending is forced off for the palette
// target, since blending an 8-bit palette index would produce a meaningless index rather than a
// blended color.
ff_dx12_draw_state_flags ff_dx12_draw_state_make_flags(DXGI_FORMAT target_format, bool has_depth,
    bool transparent, bool pre_multiplied_alpha);

// Binds the root signature, sampler table and topology. Call once per begin_draw, before any
// apply. Returns false when the state layer has no root signature.
bool ff_dx12_draw_state_bind(ff_dx12_draw_state* state, ff_dx12_commands* commands);

// Sets the pipeline state for one bucket, creating and caching the permutation on first use.
// flags must come from ff_dx12_draw_state_make_flags: the blend and target fields each have a
// two-bit pattern that no format maps to, and a hand-built value using one would quietly alias
// onto a different permutation rather than being rejected.
bool ff_dx12_draw_state_apply(ff_dx12_draw_state* state, ff_dx12_commands* commands,
    ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags);

// True for the permutation keys make_flags can actually produce. Used to assert callers are not
// passing a reserved bit pattern.
bool ff_dx12_draw_state_flags_valid(ff_dx12_draw_state_flags flags);

// Number of pipeline permutations currently cached, for tests that need to see the matrix fill in
// and empty out.
size_t ff_dx12_draw_state_pipeline_count(const ff_dx12_draw_state* state);

// The pixel shader a permutation selects. The palette target uses a different pixel shader than
// the color targets, and that choice is otherwise invisible from outside: two permutations that
// differ only by shader still produce two distinct pipeline objects, so comparing pipeline
// pointers cannot tell the shader selection apart from the render target format.
ff_dx12_shader ff_dx12_draw_state_bucket_ps(const ff_dx12_draw_state* state,
    ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags);

// Device reset: every cached pipeline and the root signature belong to the object cache, which
// releases them itself. This only drops the now-dangling pointers, and reset rebuilds them.
void internal_ff_dx12_draw_state_before_reset(ff_dx12_draw_state* state);
bool internal_ff_dx12_draw_state_reset(ff_dx12_draw_state* state);
