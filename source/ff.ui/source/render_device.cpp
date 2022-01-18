#include "pch.h"
#include "render_device.h"
#include "render_target.h"
#include "texture.h"
#include "ui.h"

static void set_filter(Noesis::MinMagFilter::Enum minmag, Noesis::MipFilter::Enum mip, D3D12_SAMPLER_DESC& desc)
{
    switch (minmag)
    {
        case Noesis::MinMagFilter::Nearest:
            switch (mip)
            {
                case Noesis::MipFilter::Disabled:
                    desc.MaxLOD = 0;
                    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                    break;

                case Noesis::MipFilter::Nearest:
                    desc.MaxLOD = D3D12_FLOAT32_MAX;
                    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                    break;

                case Noesis::MipFilter::Linear:
                    desc.MaxLOD = D3D12_FLOAT32_MAX;
                    desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                    break;
            }
            break;

        case Noesis::MinMagFilter::Linear:
            switch (mip)
            {
                case Noesis::MipFilter::Disabled:
                    desc.MaxLOD = 0;
                    desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    break;

                case Noesis::MipFilter::Nearest:
                    desc.MaxLOD = D3D12_FLOAT32_MAX;
                    desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    break;

                case Noesis::MipFilter::Linear:
                    desc.MaxLOD = D3D12_FLOAT32_MAX;
                    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    break;
            }
            break;
    }
}

static void set_address(Noesis::WrapMode::Enum mode, D3D12_SAMPLER_DESC& desc)
{
    switch (mode)
    {
        case Noesis::WrapMode::ClampToEdge:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            break;

        case Noesis::WrapMode::ClampToZero:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            break;

        case Noesis::WrapMode::Repeat:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            break;

        case Noesis::WrapMode::MirrorU:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            break;

        case Noesis::WrapMode::MirrorV:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            break;

        case Noesis::WrapMode::Mirror:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            break;
    }
}

static std::pair<const char*, UINT> semantic(uint32_t attr)
{
    switch (attr)
    {
        case Noesis::Shader::Vertex::Format::Attr::Pos: return{ "POSITION", 0 };
        case Noesis::Shader::Vertex::Format::Attr::Color: return { "COLOR", 0 };
        case Noesis::Shader::Vertex::Format::Attr::Tex0: return { "TEXCOORD", 0 };
        case Noesis::Shader::Vertex::Format::Attr::Tex1: return { "TEXCOORD", 1 };
        case Noesis::Shader::Vertex::Format::Attr::Coverage: return { "COVERAGE", 0 };
        case Noesis::Shader::Vertex::Format::Attr::Rect: return { "RECT", 0 };
        case Noesis::Shader::Vertex::Format::Attr::Tile: return { "TILE", 0 };
        case Noesis::Shader::Vertex::Format::Attr::ImagePos: return { "IMAGE_POSITION", 0 };
        default: assert(false); return {};
    }
}

static DXGI_FORMAT format(uint32_t type)
{
    switch (type)
    {
        case Noesis::Shader::Vertex::Format::Attr::Type::Float: return DXGI_FORMAT_R32_FLOAT;
        case Noesis::Shader::Vertex::Format::Attr::Type::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case Noesis::Shader::Vertex::Format::Attr::Type::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Noesis::Shader::Vertex::Format::Attr::Type::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Noesis::Shader::Vertex::Format::Attr::Type::UShort4Norm: return DXGI_FORMAT_R16G16B16A16_UNORM;
        default: assert(false); return {};
    }
}

static void fill_elements(ff::stack_vector<D3D12_INPUT_ELEMENT_DESC, 16>& elements, uint32_t format)
{
    uint32_t attributes = Noesis::AttributesForFormat[format];

    for (uint32_t i = 0; i < Noesis::Shader::Vertex::Format::Attr::Count; i++)
    {
        if (attributes & (1 << i))
        {
            D3D12_INPUT_ELEMENT_DESC& element = elements.emplace_back();
            element.SemanticName = ::semantic(i).first;
            element.SemanticIndex = ::semantic(i).second;
            element.Format = ::format(Noesis::TypeForAttr[i]);
            element.InputSlot = 0;
            element.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
            element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            element.InstanceDataStepRate = 0;
        }
    }
}

static void rasterizer_desc(D3D12_RASTERIZER_DESC& desc, Noesis::RenderState state)
{
    desc = {};
    desc.CullMode = D3D12_CULL_MODE_NONE;

    if (state.f.wireframe)
    {
        desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
    }
    else
    {
        desc.FillMode = D3D12_FILL_MODE_SOLID;
    }
}

static void blend_desc(D3D12_BLEND_DESC& desc, Noesis::RenderState state)
{
    desc = {};
    desc.AlphaToCoverageEnable = false;
    desc.IndependentBlendEnable = false;

    if (state.f.colorEnable)
    {
        desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        switch (state.f.blendMode)
        {
            case Noesis::BlendMode::Src:
                desc.RenderTarget[0].BlendEnable = false;
                break;

            case Noesis::BlendMode::SrcOver:
                desc.RenderTarget[0].BlendEnable = true;
                desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                break;

            case Noesis::BlendMode::SrcOver_Multiply:
                desc.RenderTarget[0].BlendEnable = true;
                desc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
                desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                break;

            case Noesis::BlendMode::SrcOver_Screen:
                desc.RenderTarget[0].BlendEnable = true;
                desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_COLOR;
                desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                break;

            case Noesis::BlendMode::SrcOver_Additive:
                desc.RenderTarget[0].BlendEnable = true;
                desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
                desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                break;

            case Noesis::BlendMode::SrcOver_Dual:
                desc.RenderTarget[0].BlendEnable = true;
                desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_COLOR;
                desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
                desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC1_ALPHA;
                break;
        }
    }
    else
    {
        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
    }
}

static void depth_stencil_desc(D3D12_DEPTH_STENCIL_DESC& desc, Noesis::RenderState state)
{
    desc = {};
    desc.StencilReadMask = 0xFF;
    desc.StencilWriteMask = 0xFF;
    desc.DepthEnable = false;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    desc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    desc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;

    switch (state.f.stencilMode)
    {
        case Noesis::StencilMode::Disabled:
            desc.StencilEnable = false;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            break;

        case Noesis::StencilMode::Equal_Keep:
            desc.StencilEnable = true;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            break;

        case Noesis::StencilMode::Equal_Incr:
            desc.StencilEnable = true;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
            desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
            break;

        case Noesis::StencilMode::Equal_Decr:
            desc.StencilEnable = true;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_DECR;
            desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_DECR;
            break;

        case Noesis::StencilMode::Clear:
            desc.StencilEnable = true;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_ZERO;
            desc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace.StencilPassOp = D3D12_STENCIL_OP_ZERO;
            break;
    }
}

static const std::string_view pixel_shaders[] =
{
    "Noesis.RGBA_PS",
    "Noesis.Mask_PS",
    "Noesis.Clear_PS",

    "Noesis.PathSolid_PS",
    "Noesis.PathLinear_PS",
    "Noesis.PathRadial_PS",
    "Noesis.PathPattern_PS",
    "Noesis.PathPatternClamp_PS",
    "Noesis.PathPatternRepeat_PS",
    "Noesis.PathPatternMirrorU_PS",
    "Noesis.PathPatternMirrorV_PS",
    "Noesis.PathPatternMirror_PS",

    "Noesis.PathAASolid_PS",
    "Noesis.PathAALinear_PS",
    "Noesis.PathAARadial_PS",
    "Noesis.PathAAPattern_PS",
    "Noesis.PathAAPatternClamp_PS",
    "Noesis.PathAAPatternRepeat_PS",
    "Noesis.PathAAPatternMirrorU_PS",
    "Noesis.PathAAPatternMirrorV_PS",
    "Noesis.PathAAPatternMirror_PS",

    "Noesis.SDFSolid_PS",
    "Noesis.SDFLinear_PS",
    "Noesis.SDFRadial_PS",
    "Noesis.SDFPattern_PS",
    "Noesis.SDFPatternClamp_PS",
    "Noesis.SDFPatternRepeat_PS",
    "Noesis.SDFPatternMirrorU_PS",
    "Noesis.SDFPatternMirrorV_PS",
    "Noesis.SDFPatternMirror_PS",

    "Noesis.SDFLCDSolid_PS",
    "Noesis.SDFLCDLinear_PS",
    "Noesis.SDFLCDRadial_PS",
    "Noesis.SDFLCDPattern_PS",
    "Noesis.SDFLCDPatternClamp_PS",
    "Noesis.SDFLCDPatternRepeat_PS",
    "Noesis.SDFLCDPatternMirrorU_PS",
    "Noesis.SDFLCDPatternMirrorV_PS",
    "Noesis.SDFLCDPatternMirror_PS",

    "Noesis.OpacitySolid_PS",
    "Noesis.OpacityLinear_PS",
    "Noesis.OpacityRadial_PS",
    "Noesis.OpacityPattern_PS",
    "Noesis.OpacityPatternClamp_PS",
    "Noesis.OpacityPatternRepeat_PS",
    "Noesis.OpacityPatternMirrorU_PS",
    "Noesis.OpacityPatternMirrorV_PS",
    "Noesis.OpacityPatternMirror_PS",

    "Noesis.Upsample_PS",
    "Noesis.Downsample_PS",

    "Noesis.Shadow_PS",
    "Noesis.Blur_PS",
};

static const std::string_view vertex_shaders[] =
{
    "Noesis.Pos_VS",
    "Noesis.PosColor_VS",
    "Noesis.PosTex0_VS",
    "Noesis.PosTex0Rect_VS",
    "Noesis.PosTex0RectTile_VS",
    "Noesis.PosColorCoverage_VS",
    "Noesis.PosTex0Coverage_VS",
    "Noesis.PosTex0CoverageRect_VS",
    "Noesis.PosTex0CoverageRectTile_VS",
    "Noesis.PosColorTex1_SDF_VS",
    "Noesis.PosTex0Tex1_SDF_VS",
    "Noesis.PosTex0Tex1Rect_SDF_VS",
    "Noesis.PosTex0Tex1RectTile_SDF_VS",
    "Noesis.PosColorTex1_VS",
    "Noesis.PosTex0Tex1_VS",
    "Noesis.PosTex0Tex1Rect_VS",
    "Noesis.PosTex0Tex1RectTile_VS",
    "Noesis.PosColorTex0Tex1_VS",
    "Noesis.PosTex0Tex1_Downsample_VS",
    "Noesis.PosColorTex1Rect_VS",
    "Noesis.PosColorTex0RectImagePos_VS",
};

static const std::string_view vertex_shaders_srgb[] =
{
    "Noesis.Pos_VS",
    "Noesis.PosColor_SRGB_VS",
    "Noesis.PosTex0_VS",
    "Noesis.PosTex0Rect_VS",
    "Noesis.PosTex0RectTile_VS",
    "Noesis.PosColorCoverage_SRGB_VS",
    "Noesis.PosTex0Coverage_VS",
    "Noesis.PosTex0CoverageRect_VS",
    "Noesis.PosTex0CoverageRectTile_VS",
    "Noesis.PosColorTex1_SDF_SRGB_VS",
    "Noesis.PosTex0Tex1_SDF_VS",
    "Noesis.PosTex0Tex1Rect_SDF_VS",
    "Noesis.PosTex0Tex1RectTile_SDF_VS",
    "Noesis.PosColorTex1_SRGB_VS",
    "Noesis.PosTex0Tex1_VS",
    "Noesis.PosTex0Tex1Rect_VS",
    "Noesis.PosTex0Tex1RectTile_VS",
    "Noesis.PosColorTex0Tex1_SRGB_VS",
    "Noesis.PosTex0Tex1_Downsample_VS",
    "Noesis.PosColorTex1Rect_SRGB_VS",
    "Noesis.PosColorTex0RectImagePos_SRGB_VS",
};

ff::internal::ui::render_device::render_device(bool srgb)
    : commands(nullptr)
    , target_format(DXGI_FORMAT_UNKNOWN)
    , vertex_buffer(ff::dxgi::buffer_type::vertex)
    , index_buffer(ff::dxgi::buffer_type::index)
    , constant_buffers
    {
        ff::dx12::buffer(ff::dxgi::buffer_type::constant),
        ff::dx12::buffer(ff::dxgi::buffer_type::constant),
        ff::dx12::buffer(ff::dxgi::buffer_type::constant),
        ff::dx12::buffer(ff::dxgi::buffer_type::constant),
        ff::dx12::buffer(ff::dxgi::buffer_type::constant),
    }
    , samplers_cpu(ff::dx12::cpu_sampler_descriptors().alloc_range(64))
    , empty_views_cpu(ff::dx12::cpu_buffer_descriptors().alloc_range(3))
{
    this->caps.linearRendering = srgb;
    this->reset();

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::internal::ui::render_device::~render_device()
{
    ff::dx12::remove_device_child(this);
}

ff::dxgi::command_context_base& ff::internal::ui::render_device::render_begin()
{
    assert_msg(!this->commands, "render_device::render_begin called while already rendering");

    this->commands = &ff::dx12::frame_commands();
    this->commands->primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    this->commands->root_signature(this->root_signature.Get());

    return *this->commands;
}

ff::dxgi::command_context_base& ff::internal::ui::render_device::render_begin(
    ff::dxgi::target_base& target,
    ff::dxgi::depth_base& depth,
    const ff::rect_size& view_rect)
{
    this->render_begin();

    assert(depth.size() == target.size().physical_pixel_size());
    depth.clear_stencil(*this->commands, 0);

    D3D12_VIEWPORT viewport{};
    viewport.MaxDepth = 1;
    viewport.TopLeftX = static_cast<float>(view_rect.left);
    viewport.TopLeftY = static_cast<float>(view_rect.top);
    viewport.Width = static_cast<float>(view_rect.width());
    viewport.Height = static_cast<float>(view_rect.height());

    ff::dxgi::target_base* single_target = &target;
    this->target_format = target.format();
    this->commands->targets(&single_target, 1, &depth);
    this->commands->viewports(&viewport, 1);

    return *this->commands;
}

void ff::internal::ui::render_device::render_end()
{
    this->commands = nullptr;
    this->target_format = DXGI_FORMAT_UNKNOWN;
}

const Noesis::DeviceCaps& ff::internal::ui::render_device::GetCaps() const
{
    return this->caps;
}

Noesis::Ptr<Noesis::RenderTarget> ff::internal::ui::render_device::CreateRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sample_count, bool needs_stencil)
{
    std::string_view name(label ? label : "");
    return *new ff::internal::ui::render_target(static_cast<size_t>(width), static_cast<size_t>(height), static_cast<size_t>(sample_count), this->caps.linearRendering, needs_stencil, name);
}

Noesis::Ptr<Noesis::RenderTarget> ff::internal::ui::render_device::CloneRenderTarget(const char* label, Noesis::RenderTarget* surface)
{
    std::string_view name(label ? label : "");
    return ff::internal::ui::render_target::get(surface)->clone(name);
}

Noesis::Ptr<Noesis::Texture> ff::internal::ui::render_device::CreateTexture(const char* label, uint32_t width, uint32_t height, uint32_t mip_count, Noesis::TextureFormat::Enum format, const void** data)
{
    std::string_view name(label ? label : "");
    DXGI_FORMAT format2 = (format == Noesis::TextureFormat::R8) ? DXGI_FORMAT_R8_UNORM : (this->caps.linearRendering ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);
    std::shared_ptr<ff::texture> texture;

    DirectX::ScratchImage scratch;
    if (FAILED(scratch.Initialize2D(format2, width, height, 1, mip_count)))
    {
        assert(false);
        return nullptr;
    }

    if (data == nullptr)
    {
        std::memset(scratch.GetPixels(), 0, scratch.GetPixelsSize());
    }
    else
    {
        const uint32_t bpp = (format == DXGI_FORMAT_R8_UNORM) ? 1 : 4;

        for (size_t i = 0; i < mip_count; i++)
        {
            const DirectX::Image* image = scratch.GetImage(i, 0, 0);
            uint8_t* dest = image->pixels;
            const uint8_t* source = reinterpret_cast<const uint8_t*>(data[i]);
            size_t source_pitch = (width >> i) * bpp;
            size_t dest_pitch = image->rowPitch;

            for (size_t y = 0; y < image->height; y++, dest += dest_pitch, source += source_pitch)
            {
                std::memcpy(dest, source, bpp * image->width);
            }
        }
    }

    auto dxgi_texture = ff::dxgi_client().create_static_texture(std::make_shared<DirectX::ScratchImage>(std::move(scratch)), ff::dxgi::sprite_type::unknown);
    texture = std::make_shared<ff::texture>(dxgi_texture);

    return *new ff::internal::ui::texture(texture, name);
}

void ff::internal::ui::render_device::UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data)
{
    ff::dxgi::texture_base* texture2 = ff::internal::ui::texture::get(texture)->internal_texture()->dxgi_texture().get();
    ff::point_size pos(x, y);

    size_t row_pitch, slice_pitch;
    DirectX::ComputePitch(texture2->format(), static_cast<size_t>(width), static_cast<size_t>(height), row_pitch, slice_pitch);

    DirectX::Image image_data
    {
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        texture2->format(),
        row_pitch,
        slice_pitch,
        reinterpret_cast<uint8_t*>(const_cast<void*>(data)),
    };

    texture2->update(*this->commands, 0, static_cast<size_t>(level), pos, image_data);
}

void ff::internal::ui::render_device::BeginOnscreenRender()
{}

void ff::internal::ui::render_device::EndOnscreenRender()
{}

void ff::internal::ui::render_device::BeginOffscreenRender()
{
    this->BeginOnscreenRender();
}

void ff::internal::ui::render_device::EndOffscreenRender()
{
    this->EndOnscreenRender();
}

void ff::internal::ui::render_device::SetRenderTarget(Noesis::RenderTarget* surface)
{
    ff::internal::ui::render_target* surface2 = ff::internal::ui::render_target::get(surface);
    ff::point_float size = surface2->msaa_texture()->dxgi_texture()->size().cast<float>();
    ff::dxgi::target_base* single_target = surface2->msaa_target().get();

    D3D12_VIEWPORT viewport{};
    viewport.Width = size.x;
    viewport.Height = size.y;
    viewport.MaxDepth = 1.0f;

    this->target_format = single_target->format();
    this->commands->targets(&single_target, 1, surface2->depth().get());
    this->commands->viewports(&viewport, 1);
}

void ff::internal::ui::render_device::ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t tile_count)
{
    ff::internal::ui::render_target* surface2 = ff::internal::ui::render_target::get(surface);
    if (surface2->msaa_texture() != surface2->resolved_texture())
    {
        ff::point_size size = surface2->msaa_texture()->dxgi_texture()->size();

        for (uint32_t i = 0; i < tile_count; i++)
        {
            const Noesis::Tile& tile = tiles[i];
            ff::point_size dst(static_cast<size_t>(tile.x), size.y - static_cast<size_t>(tile.y + tile.height));
            ff::rect_size src(dst, dst + ff::point_t<uint32_t>(tile.width, tile.height).cast<size_t>());

            ff::dx12::texture& resolved_texture = ff::dx12::texture::get(*surface2->resolved_texture()->dxgi_texture());
            ff::dx12::texture& msaa_texture = ff::dx12::texture::get(*surface2->msaa_texture()->dxgi_texture());
            this->commands->resolve(*resolved_texture.dx12_resource(), 0, dst, *msaa_texture.dx12_resource(), 0, src);
        }
    }
}

void* ff::internal::ui::render_device::MapVertices(uint32_t bytes)
{
    return this->vertex_buffer.map(*this->commands, static_cast<size_t>(bytes));
}

void ff::internal::ui::render_device::UnmapVertices()
{
    this->vertex_buffer.unmap();
}

void* ff::internal::ui::render_device::MapIndices(uint32_t bytes)
{
    return this->index_buffer.map(*this->commands, static_cast<size_t>(bytes));
}

void ff::internal::ui::render_device::UnmapIndices()
{
    this->index_buffer.unmap();
}

void ff::internal::ui::render_device::DrawBatch(const Noesis::Batch& batch)
{
    if (batch.pixelShader || !this->index_buffer || !this->vertex_buffer)
    {
        assert(false); // custom shaders aren't supported yet
        return;
    }

    // Set pipeline state
    {
        auto [pipeline_state, stride] = this->pipeline_state_and_stride(batch.shader.v, batch.renderState.v);
        D3D12_VERTEX_BUFFER_VIEW vertex_view = this->vertex_buffer.vertex_view(stride, static_cast<uint64_t>(batch.vertexOffset), static_cast<size_t>(batch.numVertices));
        ff::dx12::buffer_base* single_vertex_buffer = &this->vertex_buffer;

        this->commands->pipeline_state(pipeline_state);
        this->commands->stencil(batch.stencilRef);
        this->commands->vertex_buffers(&single_vertex_buffer, &vertex_view, 0, 1);
        this->commands->index_buffer(this->index_buffer, this->index_buffer.index_view());
    }

    // Update constant buffers
    for (size_t i = 0; i < 4; i++)
    {
        const Noesis::UniformData& data = (i < 2) ? batch.vertexUniforms[i] : batch.pixelUniforms[i - 2];
        if (data.values && data.numDwords)
        {
            ff::dx12::buffer& buffer = this->constant_buffers[i];
            buffer.update(*this->commands, data.values, data.numDwords * sizeof(DWORD));
            this->commands->root_cbv(i, *buffer.resource(), buffer.gpu_address());
        }
    }

    // Palette constant buffer
    {
        struct
        {
            ff::point_float pattern_size;
            ff::point_float pattern_inverse_size;
            unsigned int palette_row;
            unsigned int padding[3];
        } pb2{};

        ff::internal::ui::texture* pattern_texture = ff::internal::ui::texture::get(batch.pattern);
        pb2.pattern_size = pattern_texture ? pattern_texture->internal_texture()->dxgi_texture()->size().cast<float>() : ff::point_float(0, 0);
        pb2.pattern_inverse_size = pattern_texture ? ff::point_float(1, 1) / pb2.pattern_size : ff::point_float(0, 0);
        pb2.palette_row = static_cast<unsigned int>(ff::ui::global_palette() ? ff::ui::global_palette()->current_row() : 0);

        this->constant_buffers[4].update(*this->commands, &pb2, sizeof(pb2));
        this->commands->root_cbv(4, *this->constant_buffers[4].resource(), this->constant_buffers[4].gpu_address());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE empty_view = this->empty_views_cpu.cpu_handle(1);
    D3D12_CPU_DESCRIPTOR_HANDLE empty_palette_view = this->empty_views_cpu.cpu_handle(2);
    D3D12_CPU_DESCRIPTOR_HANDLE empty_sampler = this->samplers_cpu.cpu_handle(0);

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 7> texture_views
    {
        empty_view, empty_view, empty_view, empty_view, empty_view, empty_palette_view, empty_view
    };

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 5> samplers
    {
        empty_sampler, empty_sampler, empty_sampler, empty_sampler, empty_sampler
    };

    if (batch.pattern)
    {
        ff::dx12::texture& t = ff::dx12::texture::get(*ff::internal::ui::texture::get(batch.pattern)->internal_texture()->dxgi_texture());
        this->commands->resource_state(*t.dx12_resource_updated(*this->commands), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        bool palette = ff::dxgi::palette_format(t.format());
        if (palette && ff::ui::global_palette())
        {
            ff::dx12::texture& pt = ff::dx12::texture::get(*ff::ui::global_palette()->data()->texture());
            this->commands->resource_state(*pt.dx12_resource_updated(*this->commands), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            texture_views[6] = pt.dx12_texture_view();
        }

        texture_views[palette ? 5 : 0] = t.dx12_texture_view();
        samplers[0] = this->samplers_cpu.cpu_handle(batch.patternSampler.v);
    }

    auto set_texture = [this, &texture_views, &samplers](size_t index, Noesis::Texture* batchTexture, Noesis::SamplerState batchSampler)
    {
        if (batchTexture)
        {
            ff::dx12::texture& t = ff::dx12::texture::get(*ff::internal::ui::texture::get(batchTexture)->internal_texture()->dxgi_texture());
            this->commands->resource_state(*t.dx12_resource_updated(*this->commands), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            texture_views[index] = t.dx12_texture_view();
            samplers[index] = this->samplers_cpu.cpu_handle(batchSampler.v);
        }
    };

    set_texture(1, batch.ramps, batch.rampsSampler);
    set_texture(2, batch.image, batch.imageSampler);
    set_texture(3, batch.glyphs, batch.glyphsSampler);
    set_texture(4, batch.shadow, batch.shadowSampler);

    static const UINT ones[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

    // Copy texture descriptors
    {
        ff::dx12::descriptor_range texture_views_gpu = ff::dx12::gpu_view_descriptors().alloc_range(texture_views.size(), this->commands->next_fence_value());
        D3D12_CPU_DESCRIPTOR_HANDLE dest = texture_views_gpu.cpu_handle(0);
        UINT dest_size = static_cast<UINT>(texture_views.size());
        ff::dx12::device()->CopyDescriptors(1, &dest, &dest_size, dest_size, texture_views.data(), ones, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        this->commands->root_descriptors(5, texture_views_gpu);
    }

    // Copy sampler descriptors
    {
        ff::dx12::descriptor_range samplers_gpu = ff::dx12::gpu_sampler_descriptors().alloc_range(samplers.size(), this->commands->next_fence_value());
        D3D12_CPU_DESCRIPTOR_HANDLE dest = samplers_gpu.cpu_handle(0);
        UINT dest_size = static_cast<UINT>(samplers.size());
        ff::dx12::device()->CopyDescriptors(1, &dest, &dest_size, dest_size, samplers.data(), ones, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        this->commands->root_descriptors(6, samplers_gpu);
    }

    this->commands->draw(batch.startIndex, batch.numIndices, 0);
}

void ff::internal::ui::render_device::before_reset()
{
    this->root_signature.Reset();

    for (shader_t& shader : this->shaders)
    {
        shader = {};
    }
}

bool ff::internal::ui::render_device::reset()
{
    // null texture descriptors
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC rgb_desc{};
        rgb_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rgb_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        rgb_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        rgb_desc.Texture2D.MipLevels = 1;

        D3D12_SHADER_RESOURCE_VIEW_DESC palette_desc{};
        palette_desc.Format = DXGI_FORMAT_R8_UINT;
        palette_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        palette_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        palette_desc.Texture2D.MipLevels = 1;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cb_desc{};

        ff::dx12::device()->CreateConstantBufferView(&cb_desc, this->empty_views_cpu.cpu_handle(0));
        ff::dx12::device()->CreateShaderResourceView(nullptr, &rgb_desc, this->empty_views_cpu.cpu_handle(1));
        ff::dx12::device()->CreateShaderResourceView(nullptr, &palette_desc, this->empty_views_cpu.cpu_handle(2));
    }

    this->init_samplers();
    this->init_root_signature();
    this->init_shaders();

    return true;
}

void ff::internal::ui::render_device::init_samplers()
{
    D3D12_SAMPLER_DESC desc{};
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.MipLODBias = -0.75f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MinLOD = -D3D12_FLOAT32_MAX;

    for (uint8_t minmag = 0; minmag < Noesis::MinMagFilter::Count; minmag++)
    {
        for (uint8_t mip = 0; mip < Noesis::MipFilter::Count; mip++)
        {
            ::set_filter(Noesis::MinMagFilter::Enum(minmag), Noesis::MipFilter::Enum(mip), desc);

            for (uint8_t uv = 0; uv < Noesis::WrapMode::Count; uv++)
            {
                ::set_address(Noesis::WrapMode::Enum(uv), desc);

                Noesis::SamplerState s = { { uv, minmag, mip } };
                ff::dx12::device()->CreateSampler(&desc, this->samplers_cpu.cpu_handle(static_cast<size_t>(s.v)));
            }
        }
    }
}

void ff::internal::ui::render_device::init_root_signature()
{
    CD3DX12_DESCRIPTOR_RANGE1 ps_textures;
    ps_textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);

    CD3DX12_DESCRIPTOR_RANGE1 ps_samplers;
    ps_samplers.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 5, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0);

    std::array<CD3DX12_ROOT_PARAMETER1, 7> params;
    params[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    params[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    params[2].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    params[3].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    params[4].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    params[5].InitAsDescriptorTable(1, &ps_textures, D3D12_SHADER_VISIBILITY_PIXEL);
    params[6].InitAsDescriptorTable(1, &ps_samplers, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
    D3D12_ROOT_SIGNATURE_DESC1& desc = versioned_desc.Desc_1_1;
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

    desc.NumParameters = static_cast<UINT>(params.size());
    desc.pParameters = params.data();

    this->root_signature = ff::dx12::get_object_cache().root_signature(versioned_desc);
}

void ff::internal::ui::render_device::init_shaders()
{
    for (size_t i = 0; i < _countof(this->shaders); i++)
    {
        auto vs = static_cast<Noesis::Shader::Vertex::Enum>(Noesis::VertexForShader[i]);
        auto format = static_cast<Noesis::Shader::Vertex::Format::Enum>(Noesis::FormatForVertex[vs]);
        this->shaders[i].stride = Noesis::SizeForFormat[format];
    }
}

std::pair<ID3D12PipelineStateX*, size_t> ff::internal::ui::render_device::pipeline_state_and_stride(size_t shader_index, uint8_t render_state)
{
    uint32_t pipeline_lookup = (static_cast<uint32_t>(render_state) << 24) ^ static_cast<uint32_t>(this->target_format);
    shader_t& shader = this->shaders[shader_index];
    auto i = shader.pipeline_states.find(pipeline_lookup);

    if (i == shader.pipeline_states.end())
    {
        size_t vertex_index = Noesis::VertexForShader[shader_index];
        std::string pixel_shader_name(::pixel_shaders[shader_index]);
        std::string vertex_shader_name = this->caps.linearRendering
            ? std::string(::vertex_shaders_srgb[vertex_index])
            : std::string(::vertex_shaders[vertex_index]);

        uint32_t format = Noesis::FormatForVertex[vertex_index];
        ff::stack_vector<D3D12_INPUT_ELEMENT_DESC, 16> elements;
        ::fill_elements(elements, format);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = this->root_signature.Get();
        desc.VS = ff::dx12::get_object_cache().shader(vertex_shader_name);
        desc.PS = ff::dx12::get_object_cache().shader(pixel_shader_name);
        desc.InputLayout.pInputElementDescs = elements.data();
        desc.InputLayout.NumElements = static_cast<UINT>(elements.size());
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = this->target_format;
        desc.DSVFormat = ff::dx12::depth::FORMAT;
        desc.SampleDesc.Count = 1;
        desc.SampleMask = 0xFFFFFFFF;

        Noesis::Shader shader_v;
        shader_v.v = static_cast<uint8_t>(shader_index);
        Noesis::RenderState render_state_v;
        render_state_v.v = render_state;
        assert(this->IsValidState(shader_v, render_state_v));

        ::blend_desc(desc.BlendState, render_state_v);
        ::rasterizer_desc(desc.RasterizerState, render_state_v);
        ::depth_stencil_desc(desc.DepthStencilState, render_state_v);

        i = shader.pipeline_states.try_emplace(pipeline_lookup, ff::dx12::get_object_cache().pipeline_state(desc)).first;
    }

    return std::make_pair(i->second.Get(), shader.stride);
}
