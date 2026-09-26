#include "pch.h"
#include "base/assert.h"
#include "dx12/dx12_commands.h"
#include "dx12/dx12_depth.h"
#include "dx12/dx12_descriptor_allocator.h"
#include "dx12/dx12_draw_state.h"
#include "dx12/dx12_globals.h"
#include "dx12/dx12_object_cache.h"

static_assert(sizeof(ff_dx12_vs_constants_0) == 80, "vs_constants_0 must match the HLSL cbuffer");
static_assert(FF_DX12_VS_CONSTANTS_0_DWORD_COUNT * sizeof(uint32_t) + sizeof(float) * 2 == sizeof(ff_dx12_vs_constants_0),
    "vs_constants_0 dword count must cover everything except the trailing padding");
static_assert(sizeof(ff_dx12_vs_constants_1) == FF_DX12_MAX_TRANSFORM_MATRIXES * 64, "vs_constants_1 must match the HLSL cbuffer");
static_assert(sizeof(ff_dx12_ps_constants_0) == FF_DX12_MAX_PALETTE_TEXTURES * 16, "ps_constants_0 must match the HLSL cbuffer");

#define FF_DX12_VERTEX_SLOT 0
#define FF_DX12_INSTANCE_SLOT 1

#define FF_DX12_VERTEX_ELEMENT(semantic, index, format) \
    { semantic, index, format, FF_DX12_VERTEX_SLOT, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }

#define FF_DX12_INSTANCE_ELEMENT(semantic, index, format) \
    { semantic, index, format, FF_DX12_INSTANCE_SLOT, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }

static const D3D12_INPUT_ELEMENT_DESC s_sprite_elements[] =
{
    FF_DX12_INSTANCE_ELEMENT("RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSROT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("INDEXES", 0, DXGI_FORMAT_R32_UINT),
};

static const D3D12_INPUT_ELEMENT_DESC s_line_elements[] =
{
    FF_DX12_INSTANCE_ELEMENT("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 1, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 2, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 3, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("THICKNESS", 1, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("INDEX", 0, DXGI_FORMAT_R32_UINT),
};

static const D3D12_INPUT_ELEMENT_DESC s_triangle_elements[] =
{
    FF_DX12_INSTANCE_ELEMENT("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 1, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 2, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("INDEX", 0, DXGI_FORMAT_R32_UINT),
};

static const D3D12_INPUT_ELEMENT_DESC s_rectangle_elements[] =
{
    FF_DX12_INSTANCE_ELEMENT("RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("INDEX", 0, DXGI_FORMAT_R32_UINT),
};

static const D3D12_INPUT_ELEMENT_DESC s_circle_elements[] =
{
    FF_DX12_VERTEX_ELEMENT("COSSIN", 0, DXGI_FORMAT_R32G32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
    FF_DX12_INSTANCE_ELEMENT("INDEX", 0, DXGI_FORMAT_R32_UINT),
};

typedef struct ff_dx12_draw_bucket_desc
{
    const D3D12_INPUT_ELEMENT_DESC* elements;
    size_t element_count;
    ff_dx12_shader vs;
    ff_dx12_shader ps;
    ff_dx12_shader ps_palette_out;
} ff_dx12_draw_bucket_desc;

static const ff_dx12_draw_bucket_desc s_bucket_descs[ff_dx12_draw_bucket_count] =
{
    [ff_dx12_draw_bucket_sprites] =
    {
        .elements = s_sprite_elements,
        .element_count = _countof(s_sprite_elements),
        .vs = ff_dx12_shader_vs_sprite,
        .ps = ff_dx12_shader_ps_sprite,
        .ps_palette_out = ff_dx12_shader_ps_sprite_out_palette,
    },
    [ff_dx12_draw_bucket_palette_sprites] =
    {
        .elements = s_sprite_elements,
        .element_count = _countof(s_sprite_elements),
        .vs = ff_dx12_shader_vs_sprite,
        .ps = ff_dx12_shader_ps_palette_sprite,
        .ps_palette_out = ff_dx12_shader_ps_palette_sprite_out_palette,
    },
    [ff_dx12_draw_bucket_lines] =
    {
        .elements = s_line_elements,
        .element_count = _countof(s_line_elements),
        .vs = ff_dx12_shader_vs_line,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
    [ff_dx12_draw_bucket_triangles] =
    {
        .elements = s_triangle_elements,
        .element_count = _countof(s_triangle_elements),
        .vs = ff_dx12_shader_vs_triangle,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
    [ff_dx12_draw_bucket_rectangles_filled] =
    {
        .elements = s_rectangle_elements,
        .element_count = _countof(s_rectangle_elements),
        .vs = ff_dx12_shader_vs_rectangle,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
    [ff_dx12_draw_bucket_rectangles_outline] =
    {
        .elements = s_rectangle_elements,
        .element_count = _countof(s_rectangle_elements),
        .vs = ff_dx12_shader_vs_rectangle,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
    [ff_dx12_draw_bucket_circles_filled] =
    {
        .elements = s_circle_elements,
        .element_count = _countof(s_circle_elements),
        .vs = ff_dx12_shader_vs_circle,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
    [ff_dx12_draw_bucket_circles_outline] =
    {
        .elements = s_circle_elements,
        .element_count = _countof(s_circle_elements),
        .vs = ff_dx12_shader_vs_circle,
        .ps = ff_dx12_shader_ps_color,
        .ps_palette_out = ff_dx12_shader_ps_color_out_palette,
    },
};

static D3D12_SAMPLER_DESC sampler_desc(D3D12_FILTER filter)
{
    D3D12_SAMPLER_DESC desc = { 0 };
    desc.Filter = filter;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MaxLOD = D3D12_FLOAT32_MAX;

    return desc;
}

static void alpha_blend_desc(D3D12_RENDER_TARGET_BLEND_DESC* desc)
{
    desc->BlendEnable = TRUE;
    desc->SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc->DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc->BlendOp = D3D12_BLEND_OP_ADD;
    desc->SrcBlendAlpha = D3D12_BLEND_ONE;
    desc->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    desc->BlendOpAlpha = D3D12_BLEND_OP_ADD;
}

static void pma_blend_desc(D3D12_RENDER_TARGET_BLEND_DESC* desc)
{
    desc->BlendEnable = TRUE;
    desc->SrcBlend = D3D12_BLEND_ONE;
    desc->DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc->BlendOp = D3D12_BLEND_OP_ADD;
    desc->SrcBlendAlpha = D3D12_BLEND_ONE;
    desc->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    desc->BlendOpAlpha = D3D12_BLEND_OP_ADD;
}

static void default_blend_desc(D3D12_BLEND_DESC* desc)
{
    *desc = (D3D12_BLEND_DESC){ 0 };

    for (size_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
        desc->RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        desc->RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        desc->RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        desc->RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc->RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        desc->RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc->RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        desc->RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
}

static void default_rasterizer_desc(D3D12_RASTERIZER_DESC* desc)
{
    *desc = (D3D12_RASTERIZER_DESC){ 0 };
    desc->FillMode = D3D12_FILL_MODE_SOLID;
    desc->CullMode = D3D12_CULL_MODE_NONE;
    desc->DepthClipEnable = TRUE;
    desc->ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}

static void default_depth_stencil_desc(D3D12_DEPTH_STENCIL_DESC* desc)
{
    const D3D12_DEPTH_STENCILOP_DESC stencil_op =
    {
        .StencilFailOp = D3D12_STENCIL_OP_KEEP,
        .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
        .StencilPassOp = D3D12_STENCIL_OP_KEEP,
        .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
    };

    *desc = (D3D12_DEPTH_STENCIL_DESC){ 0 };
    desc->DepthEnable = TRUE;
    desc->DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc->DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc->StencilEnable = FALSE;
    desc->StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    desc->StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    desc->FrontFace = stencil_op;
    desc->BackFace = stencil_op;
}

bool ff_dx12_draw_state_target_format_valid(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R8_UINT:
            return true;

        default:
            return false;
    }
}

ff_dx12_draw_state_flags ff_dx12_draw_state_make_flags(DXGI_FORMAT target_format, bool has_depth,
    bool transparent, bool pre_multiplied_alpha)
{
    ff_dx12_draw_state_flags flags = ff_dx12_draw_state_blend_opaque;

    if (has_depth)
    {
        flags |= ff_dx12_draw_state_depth_enabled;
    }

    // The palette target holds an index, not a color, so any blend would corrupt it. That is also
    // why the blend decision has to see the target format rather than just the transparent flag.
    if (transparent && target_format != DXGI_FORMAT_R8_UINT)
    {
        flags |= pre_multiplied_alpha ? ff_dx12_draw_state_blend_pma : ff_dx12_draw_state_blend_alpha;
    }

    if (target_format == DXGI_FORMAT_R8_UINT)
    {
        flags |= ff_dx12_draw_state_target_palette;
    }
    else if (target_format == DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        flags |= ff_dx12_draw_state_target_bgra;
    }

    return flags;
}

ff_dx12_shader ff_dx12_draw_state_bucket_ps(const ff_dx12_draw_state* state,
    ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags)
{
    FF_ASSERT_RET_VAL(state, ff_dx12_shader_count);
    FF_ASSERT_RET_VAL(bucket < ff_dx12_draw_bucket_count, ff_dx12_shader_count);

    const ff_dx12_draw_bucket_state* bucket_state = &state->buckets[bucket];

    return (flags & ff_dx12_draw_state_target_palette)
        ? bucket_state->ps_palette_out
        : bucket_state->ps;
}

static ID3D12PipelineState* create_pipeline_state(ff_dx12_draw_state* state,
    ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags)
{
    ff_dx12_draw_bucket_state* bucket_state = &state->buckets[bucket];
    const bool palette_out = (flags & ff_dx12_draw_state_target_palette) != 0;

    const D3D12_SHADER_BYTECODE vs = ff_dx12_object_cache_shader(&state->cache, bucket_state->vs);
    const D3D12_SHADER_BYTECODE ps = ff_dx12_object_cache_shader(&state->cache,
        ff_dx12_draw_state_bucket_ps(state, bucket, flags));

    // A missing .cso would otherwise be handed to the driver as a zero-length blob, which fails
    // deep inside CreatePipelineState with nothing pointing back at the shader that was missing.
    FF_CHECK_RET_VAL(vs.pShaderBytecode && vs.BytecodeLength, NULL);
    FF_CHECK_RET_VAL(ps.pShaderBytecode && ps.BytecodeLength, NULL);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = { 0 };
    desc.pRootSignature = state->root_signature;
    desc.VS = vs;
    desc.PS = ps;
    default_blend_desc(&desc.BlendState);
    desc.SampleMask = UINT_MAX;
    default_rasterizer_desc(&desc.RasterizerState);
    default_depth_stencil_desc(&desc.DepthStencilState);
    desc.InputLayout.pInputElementDescs = bucket_state->elements;
    desc.InputLayout.NumElements = (UINT)bucket_state->element_count;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.SampleDesc.Count = 1;

    switch (flags & ff_dx12_draw_state_blend_mask)
    {
        case ff_dx12_draw_state_blend_alpha:
            alpha_blend_desc(&desc.BlendState.RenderTarget[0]);
            break;

        case ff_dx12_draw_state_blend_pma:
            pma_blend_desc(&desc.BlendState.RenderTarget[0]);
            break;

        default:
            break;
    }

    if (flags & ff_dx12_draw_state_depth_enabled)
    {
        // Greater, not less: depth values are assigned front-to-back increasing, so the newest
        // instance at a given pixel wins.
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
        desc.DSVFormat = FF_DX12_DEPTH_FORMAT;
    }
    else
    {
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    }

    if (palette_out)
    {
        desc.RTVFormats[0] = DXGI_FORMAT_R8_UINT;
    }
    else if (flags & ff_dx12_draw_state_target_bgra)
    {
        desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    else
    {
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    return ff_dx12_object_cache_pipeline_state(&state->cache, &desc);
}

static bool create_samplers(ff_dx12_draw_state* state)
{
    ff_dx12_descriptor_range samplers_cpu = ff_dx12_cpu_descriptor_allocator_alloc(
        ff_dx12_cpu_sampler_descriptors(), FF_DX12_SAMPLER_COUNT);

    FF_CHECK_RET_VAL(ff_dx12_descriptor_range_valid(&samplers_cpu), false);

    const D3D12_SAMPLER_DESC point = sampler_desc(D3D12_FILTER_MIN_MAG_MIP_POINT);
    const D3D12_SAMPLER_DESC linear = sampler_desc(D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    ID3D12Device6_CreateSampler(ff_dx12_device(), &point,
        ff_dx12_descriptor_range_cpu_handle(&samplers_cpu, ff_dx12_sampler_point));
    ID3D12Device6_CreateSampler(ff_dx12_device(), &linear,
        ff_dx12_descriptor_range_cpu_handle(&samplers_cpu, ff_dx12_sampler_linear));

    ID3D12Device6_CopyDescriptorsSimple(ff_dx12_device(), FF_DX12_SAMPLER_COUNT,
        ff_dx12_descriptor_range_cpu_handle(&state->samplers_gpu, 0),
        ff_dx12_descriptor_range_cpu_handle(&samplers_cpu, 0),
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // The staging descriptors were only needed to source the copy above, which is complete on the
    // CPU by the time CopyDescriptorsSimple returns.
    ff_dx12_descriptor_range_free(&samplers_cpu);

    return true;
}

static bool create_root_signature(ff_dx12_draw_state* state)
{
    D3D12_DESCRIPTOR_RANGE1 samplers_range = { 0 };
    samplers_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplers_range.NumDescriptors = FF_DX12_SAMPLER_COUNT;
    samplers_range.BaseShaderRegister = 0;
    samplers_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE1 textures_range = { 0 };
    textures_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textures_range.NumDescriptors = FF_DX12_MAX_TEXTURES;
    textures_range.BaseShaderRegister = 0;
    textures_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    textures_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE1 palette_textures_range = { 0 };
    palette_textures_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    palette_textures_range.NumDescriptors = FF_DX12_MAX_PALETTE_TEXTURES;
    palette_textures_range.BaseShaderRegister = FF_DX12_MAX_TEXTURES;
    palette_textures_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    palette_textures_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // palette_ and palette_remap_, the two singular palette lookup textures at t64 and t65.
    D3D12_DESCRIPTOR_RANGE1 palettes_range = { 0 };
    palettes_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    palettes_range.NumDescriptors = 2;
    palettes_range.BaseShaderRegister = FF_DX12_MAX_TEXTURES + FF_DX12_MAX_PALETTE_TEXTURES;
    palettes_range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
    palettes_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 params[ff_dx12_root_param_count] = { 0 };

    params[ff_dx12_root_param_vs_constants_0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[ff_dx12_root_param_vs_constants_0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[ff_dx12_root_param_vs_constants_0].Constants.ShaderRegister = 0;
    params[ff_dx12_root_param_vs_constants_0].Constants.Num32BitValues = FF_DX12_VS_CONSTANTS_0_DWORD_COUNT;

    params[ff_dx12_root_param_vs_constants_1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[ff_dx12_root_param_vs_constants_1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[ff_dx12_root_param_vs_constants_1].Descriptor.ShaderRegister = 1;

    params[ff_dx12_root_param_ps_constants_0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[ff_dx12_root_param_ps_constants_0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[ff_dx12_root_param_ps_constants_0].Descriptor.ShaderRegister = 2;

    params[ff_dx12_root_param_samplers].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[ff_dx12_root_param_samplers].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[ff_dx12_root_param_samplers].DescriptorTable.NumDescriptorRanges = 1;
    params[ff_dx12_root_param_samplers].DescriptorTable.pDescriptorRanges = &samplers_range;

    params[ff_dx12_root_param_textures].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[ff_dx12_root_param_textures].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[ff_dx12_root_param_textures].DescriptorTable.NumDescriptorRanges = 1;
    params[ff_dx12_root_param_textures].DescriptorTable.pDescriptorRanges = &textures_range;

    params[ff_dx12_root_param_palette_textures].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[ff_dx12_root_param_palette_textures].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[ff_dx12_root_param_palette_textures].DescriptorTable.NumDescriptorRanges = 1;
    params[ff_dx12_root_param_palette_textures].DescriptorTable.pDescriptorRanges = &palette_textures_range;

    params[ff_dx12_root_param_palettes].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[ff_dx12_root_param_palettes].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[ff_dx12_root_param_palettes].DescriptorTable.NumDescriptorRanges = 1;
    params[ff_dx12_root_param_palettes].DescriptorTable.pDescriptorRanges = &palettes_range;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc = { 0 };
    versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versioned_desc.Desc_1_1.NumParameters = ff_dx12_root_param_count;
    versioned_desc.Desc_1_1.pParameters = params;
    versioned_desc.Desc_1_1.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

    state->root_signature = ff_dx12_object_cache_root_signature(&state->cache, &versioned_desc);

    return state->root_signature != NULL;
}

static bool draw_state_create(ff_dx12_draw_state* state)
{
    for (size_t i = 0; i < ff_dx12_draw_bucket_count; i++)
    {
        const ff_dx12_draw_bucket_desc* desc = &s_bucket_descs[i];

        state->buckets[i].elements = desc->elements;
        state->buckets[i].element_count = desc->element_count;
        state->buckets[i].vs = desc->vs;
        state->buckets[i].ps = desc->ps;
        state->buckets[i].ps_palette_out = desc->ps_palette_out;
    }

    FF_CHECK_RET_VAL(create_samplers(state), false);
    FF_CHECK_RET_VAL(create_root_signature(state), false);

    return true;
}

bool ff_dx12_draw_state_init(ff_dx12_draw_state* state)
{
    FF_ASSERT_RET_VAL(state, false);

    *state = (ff_dx12_draw_state){ 0 };

    ff_dx12_object_cache_init(&state->cache);

    // Pinned rather than from the ring: the sampler table is bound for every draw of every frame,
    // so a ring range would be reclaimed out from under it.
    state->samplers_gpu = ff_dx12_gpu_descriptor_allocator_alloc_pinned(
        ff_dx12_gpu_sampler_descriptors(), FF_DX12_SAMPLER_COUNT);

    if (!ff_dx12_descriptor_range_valid(&state->samplers_gpu) || !draw_state_create(state))
    {
        ff_dx12_draw_state_destroy(state);
        return false;
    }

    ff_dx12_add_device_child(&state->device_child, state, ff_dx12_device_child_type_draw_state);

    return true;
}

void ff_dx12_draw_state_destroy(ff_dx12_draw_state* state)
{
    FF_ASSERT_RET(state);

    ff_dx12_remove_device_child(&state->device_child);

    // Every pipeline and the root signature are owned by the cache, so dropping the borrowed
    // pointers first keeps them from outliving what they point at. The sampler range is ours and
    // has to go back to the allocator.
    internal_ff_dx12_draw_state_before_reset(state);
    ff_dx12_descriptor_range_free(&state->samplers_gpu);
    ff_dx12_object_cache_destroy(&state->cache);

    *state = (ff_dx12_draw_state){ 0 };
}

bool ff_dx12_draw_state_valid(const ff_dx12_draw_state* state)
{
    return state && state->root_signature != NULL;
}

ID3D12RootSignature* ff_dx12_draw_state_root_signature(ff_dx12_draw_state* state)
{
    FF_ASSERT_RET_VAL(state, NULL);

    return state->root_signature;
}

ff_dx12_descriptor_range ff_dx12_draw_state_samplers(const ff_dx12_draw_state* state)
{
    FF_ASSERT_RET_VAL(state, (ff_dx12_descriptor_range){ 0 });

    return state->samplers_gpu;
}

bool ff_dx12_draw_state_bind(ff_dx12_draw_state* state, ff_dx12_commands* commands)
{
    FF_ASSERT_RET_VAL(state && commands, false);
    FF_CHECK_RET_VAL(state->root_signature, false);

    ff_dx12_commands_root_signature(commands, state->root_signature);
    ff_dx12_commands_root_descriptors(commands, ff_dx12_root_param_samplers, &state->samplers_gpu, 0);
    ff_dx12_commands_primitive_topology(commands, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool ff_dx12_draw_state_flags_valid(ff_dx12_draw_state_flags flags)
{
    return flags < ff_dx12_draw_state_count &&
        (flags & ff_dx12_draw_state_blend_mask) != ff_dx12_draw_state_blend_mask &&
        (flags & ff_dx12_draw_state_target_mask) != ff_dx12_draw_state_target_mask;
}

bool ff_dx12_draw_state_apply(ff_dx12_draw_state* state, ff_dx12_commands* commands,
    ff_dx12_draw_bucket bucket, ff_dx12_draw_state_flags flags)
{
    FF_ASSERT_RET_VAL(state && commands, false);
    FF_ASSERT_RET_VAL(bucket < ff_dx12_draw_bucket_count, false);
    FF_ASSERT_RET_VAL(ff_dx12_draw_state_flags_valid(flags), false);
    FF_CHECK_RET_VAL(state->root_signature, false);

    ID3D12PipelineState** slot = &state->buckets[bucket].pipeline_states[flags];

    if (!*slot)
    {
        *slot = create_pipeline_state(state, bucket, flags);
        FF_CHECK_RET_VAL(*slot, false);
    }

    ff_dx12_commands_pipeline_state(commands, *slot);

    return true;
}

size_t ff_dx12_draw_state_pipeline_count(const ff_dx12_draw_state* state)
{
    FF_ASSERT_RET_VAL(state, 0);

    size_t count = 0;

    for (size_t i = 0; i < ff_dx12_draw_bucket_count; i++)
    {
        for (size_t j = 0; j < ff_dx12_draw_state_count; j++)
        {
            if (state->buckets[i].pipeline_states[j])
            {
                count++;
            }
        }
    }

    return count;
}

void internal_ff_dx12_draw_state_before_reset(ff_dx12_draw_state* state)
{
    FF_ASSERT_RET(state);

    state->root_signature = NULL;

    for (size_t i = 0; i < ff_dx12_draw_bucket_count; i++)
    {
        for (size_t j = 0; j < ff_dx12_draw_state_count; j++)
        {
            state->buckets[i].pipeline_states[j] = NULL;
        }
    }
}

bool internal_ff_dx12_draw_state_reset(ff_dx12_draw_state* state)
{
    FF_ASSERT_RET_VAL(state, false);

    // The pinned sampler range keeps its heap slots across a reset, but the descriptors written
    // into them do not survive, so the samplers are created again rather than reallocated.
    return draw_state_create(state);
}
