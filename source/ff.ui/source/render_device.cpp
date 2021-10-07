#include "pch.h"
#include "texture.h"
#include "render_device.h"
#include "render_target.h"
#include "ui.h"

#if DXVER == 11

namespace
{
    struct pixel_buffer_2
    {
        ff::point_float pattern_size;
        ff::point_float pattern_inverse_size;
        unsigned int palette_row;
        unsigned int padding[3];
    };
}

static constexpr size_t PREALLOCATED_DYNAMIC_PAGES = 1;
static constexpr size_t VS_CBUFFER0_SIZE = 16 * sizeof(float);
static constexpr size_t VS_CBUFFER1_SIZE = 4 * sizeof(float);
static constexpr size_t PS_CBUFFER0_SIZE = 12 * sizeof(float);
static constexpr size_t PS_CBUFFER1_SIZE = 128 * sizeof(float);

static constexpr uint32_t VFPos = 0;
static constexpr uint32_t VFColor = 1;
static constexpr uint32_t VFTex0 = 2;
static constexpr uint32_t VFTex1 = 4;
static constexpr uint32_t VFCoverage = 8;
static constexpr uint32_t VFRect = 16;
static constexpr uint32_t VFTile = 32;
static constexpr uint32_t VFImagePos = 64;

static const Noesis::Pair<uint32_t, uint32_t> LAYOUT_FORMATS_AND_STRIDE[] =
{
    { VFPos, 8 },
    { VFPos | VFColor, 12 },
    { VFPos | VFTex0, 16 },
    { VFPos | VFTex0 | VFRect, 24 },
    { VFPos | VFTex0 | VFRect | VFTile, 40 },
    { VFPos | VFColor | VFCoverage, 16 },
    { VFPos | VFTex0 | VFCoverage, 20 },
    { VFPos | VFTex0 | VFCoverage | VFRect, 28 },
    { VFPos | VFTex0 | VFCoverage | VFRect | VFTile, 44 },
    { VFPos | VFColor | VFTex1, 20 },
    { VFPos | VFTex0 | VFTex1, 24 },
    { VFPos | VFTex0 | VFTex1 | VFRect, 32 },
    { VFPos | VFTex0 | VFTex1 | VFRect | VFTile, 48 },
    { VFPos | VFColor | VFTex0 | VFTex1, 28 },
    { VFPos | VFColor | VFTex1 | VFRect, 28 },
    { VFPos | VFColor | VFTex0 | VFRect | VFImagePos, 44 }
};

static void set_filter(Noesis::MinMagFilter::Enum minmag, Noesis::MipFilter::Enum mip, D3D11_SAMPLER_DESC& desc)
{
    switch (minmag)
    {
        case Noesis::MinMagFilter::Nearest:
            switch (mip)
            {
                case Noesis::MipFilter::Disabled:
                    desc.MaxLOD = 0;
                    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                    break;

                case Noesis::MipFilter::Nearest:
                    desc.MaxLOD = D3D11_FLOAT32_MAX;
                    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                    break;

                case Noesis::MipFilter::Linear:
                    desc.MaxLOD = D3D11_FLOAT32_MAX;
                    desc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                    break;

                default:
                    assert(false);
            }
            break;

        case Noesis::MinMagFilter::Linear:
            switch (mip)
            {
                case Noesis::MipFilter::Disabled:
                    desc.MaxLOD = 0;
                    desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    break;

                case Noesis::MipFilter::Nearest:
                    desc.MaxLOD = D3D11_FLOAT32_MAX;
                    desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    break;

                case Noesis::MipFilter::Linear:
                    desc.MaxLOD = D3D11_FLOAT32_MAX;
                    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                    break;

                default:
                    assert(false);
            }
            break;

        default:
            assert(false);
    }
}

static void set_address(Noesis::WrapMode::Enum mode, D3D_FEATURE_LEVEL level, D3D11_SAMPLER_DESC& desc)
{
    switch (mode)
    {
        case Noesis::WrapMode::ClampToEdge:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;

        case Noesis::WrapMode::ClampToZero:
            desc.AddressU = (level >= D3D_FEATURE_LEVEL_9_3) ? D3D11_TEXTURE_ADDRESS_BORDER : D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = (level >= D3D_FEATURE_LEVEL_9_3) ? D3D11_TEXTURE_ADDRESS_BORDER : D3D11_TEXTURE_ADDRESS_CLAMP;
            break;

        case Noesis::WrapMode::Repeat:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;

        case Noesis::WrapMode::MirrorU:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            break;

        case Noesis::WrapMode::MirrorV:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;

        case Noesis::WrapMode::Mirror:
            desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;

        default:
            assert(false);
    }
}

ff::internal::ui::render_device::render_device(bool srgb)
    : null_textures{}
{
    ff_dx::add_device_child(this, ff_dx::device_reset_priority::normal);

#ifdef _DEBUG
    this->empty_texture_rgb = std::make_shared<ff::texture>(ff::point_int(1, 1));
    this->empty_texture_palette = std::make_shared<ff::texture>(ff::point_int(1, 1), ff::dxgi::PALETTE_INDEX_FORMAT);
#endif

    this->caps.centerPixelOffset = 0;
    this->caps.linearRendering = srgb;
    this->caps.subpixelRendering = false;

    this->reset();
}

ff::internal::ui::render_device::~render_device()
{
    ff_dx::remove_device_child(this);
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
    DXGI_FORMAT format2 = (format == Noesis::TextureFormat::R8) ? DXGI_FORMAT_R8_UNORM : (this->caps.linearRendering ? ff::dxgi::DEFAULT_FORMAT_SRGB : ff::dxgi::DEFAULT_FORMAT);
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

    texture = std::make_shared<ff::texture>(std::make_shared<DirectX::ScratchImage>(std::move(scratch)));

    return *new ff::internal::ui::texture(texture, name);
}

void ff::internal::ui::render_device::UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data)
{
    ff::texture* texture2 = ff::internal::ui::texture::get(texture)->internal_texture().get();
    texture2->update(0, static_cast<size_t>(level), ff::rect_t<uint32_t>(x, y, x + width, y + height).cast<int>(), data, texture2->format(), true);
}

void ff::internal::ui::render_device::BeginOnscreenRender()
{
    std::array<ID3D11Buffer*, 2> buffer_vs =
    {
        this->buffer_vertex_cb[0]->dx_buffer(),
        this->buffer_vertex_cb[1]->dx_buffer(),
    };

    std::array<ID3D11Buffer*, 3> buffer_ps =
    {
        this->buffer_pixel_cb[0]->dx_buffer(),
        this->buffer_pixel_cb[1]->dx_buffer(),
        this->buffer_pixel_cb[2]->dx_buffer(),
    };

    ff_dx::get_device_state().set_constants_vs(buffer_vs.data(), 0, buffer_vs.size());
    ff_dx::get_device_state().set_constants_ps(buffer_ps.data(), 0, buffer_ps.size());
    ff_dx::get_device_state().set_topology_ia(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ff_dx::get_device_state().set_gs(nullptr);
}

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
    this->clear_textures();

    ff::internal::ui::render_target* surface2 = ff::internal::ui::render_target::get(surface);
    ff::point_float size = surface2->msaa_texture()->size().cast<float>();
    ID3D11RenderTargetView* view = surface2->msaa_target()->view();
    ff_dx::get_device_state().set_targets(&view, 1, surface2->depth()->view());

    D3D11_VIEWPORT viewport{};
    viewport.Width = size.x;
    viewport.Height = size.y;
    viewport.MaxDepth = 1.0f;

    ff_dx::get_device_state().set_viewports(&viewport, 1);
}

void ff::internal::ui::render_device::ResolveRenderTarget(Noesis::RenderTarget* surface, const Noesis::Tile* tiles, uint32_t tile_count)
{
    ff::internal::ui::render_target* surface2 = ff::internal::ui::render_target::get(surface);

    if (surface2->msaa_texture() != surface2->resolved_texture())
    {
        size_t index_ps;
        switch (surface2->msaa_texture()->sample_count())
        {
            case 2: index_ps = 0; break;
            case 4: index_ps = 1; break;
            case 8: index_ps = 2; break;
            case 16: index_ps = 3; break;
            default: assert(false); return;
        }

        ff_dx::get_device_state().set_layout_ia(nullptr);
        ff_dx::get_device_state().set_vs(this->quad_vs.shader());
        ff_dx::get_device_state().set_ps(this->resolve_ps[index_ps].shader());

        ff_dx::get_device_state().set_raster(this->rasterizer_state_scissor.Get());
        ff_dx::get_device_state().set_blend(this->blend_states[static_cast<size_t>(Noesis::BlendMode::Src)].Get(), ff::color::none(), 0xffffffff);
        ff_dx::get_device_state().set_depth(this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Disabled)].Get(), 0);

        this->clear_textures();
        ID3D11RenderTargetView* view = surface2->resolved_target()->view();
        ff_dx::get_device_state().set_targets(&view, 1, nullptr);

        ID3D11ShaderResourceView* resourceView = surface2->msaa_texture()->view();
        ff_dx::get_device_state().set_resources_ps(&resourceView, 0, 1);

        ff::point_int size = surface2->resolved_texture()->size();

        for (uint32_t i = 0; i < tile_count; i++)
        {
            const Noesis::Tile& tile = tiles[i];

            D3D11_RECT rect;
            rect.left = tile.x;
            rect.top = size.y - (tile.y + tile.height);
            rect.right = tile.x + tile.width;
            rect.bottom = size.y - tile.y;
            ff_dx::get_device_state().set_scissors(&rect, 1);

            ff_dx::get_device_state().draw(3, 0);
        }
    }
}

void* ff::internal::ui::render_device::MapVertices(uint32_t bytes)
{
    return this->buffer_vertices->map(ff_dx::get_device_state(), bytes);
}

void ff::internal::ui::render_device::UnmapVertices()
{
    this->buffer_vertices->unmap();
}

void* ff::internal::ui::render_device::MapIndices(uint32_t bytes)
{
    return this->buffer_indices->map(ff_dx::get_device_state(), bytes);
}

void ff::internal::ui::render_device::UnmapIndices()
{
    this->buffer_indices->unmap();
}

void ff::internal::ui::render_device::DrawBatch(const Noesis::Batch& batch)
{
    assert(batch.shader.v < _countof(this->programs));
    const vertex_and_pixel_program_t& program = this->programs[batch.shader.v];
    assert(program.vertex_shader_index != -1 && program.vertex_shader_index < _countof(this->vertex_stages));
    assert(program.pixel_shader_index != -1 && program.pixel_shader_index < _countof(this->pixel_shaders));

    this->set_shaders(batch);
    this->set_buffers(batch);
    this->set_render_state(batch);
    this->set_textures(batch);

    ff_dx::get_device_state().draw_indexed(batch.numIndices, batch.startIndex, 0);
    ff_dx::get_device_state().set_resources_ps(this->null_textures.data(), 0, this->null_textures.size());
}

bool ff::internal::ui::render_device::reset()
{
    this->create_buffers();
    this->create_state_objects();
    this->create_shaders();

    return true;
}

Microsoft::WRL::ComPtr<ID3D11InputLayout> ff::internal::ui::render_device::create_layout(uint32_t format, std::string_view vertex_resource_name)
{
    D3D11_INPUT_ELEMENT_DESC descs[8];
    uint32_t element = 0;

    descs[element].InputSlot = 0;
    descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    descs[element].InstanceDataStepRate = 0;
    descs[element].SemanticName = "POSITION";
    descs[element].SemanticIndex = 0;
    descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
    element++;

    if (format & VFColor)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "COLOR";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        element++;
    }

    if (format & VFTex0)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        element++;
    }

    if (format & VFTex1)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].SemanticIndex = 1;
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        element++;
    }

    if (format & VFCoverage)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "COVERAGE";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R32_FLOAT;
        element++;
    }

    if (format & VFRect)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "RECT";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R16G16B16A16_UNORM;
        element++;
    }

    if (format & VFTile)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TILE";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        element++;
    }

    if (format & VFImagePos)
    {
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "IMAGE_POSITION";
        descs[element].SemanticIndex = 0;
        descs[element].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        element++;
    }

    return ff_dx::get_object_cache().get_input_layout(vertex_resource_name, descs, element);
}

void ff::internal::ui::render_device::create_buffers()
{
    std::memset(&this->vertex_cb_hash, 0, sizeof(this->vertex_cb_hash));
    std::memset(&this->pixel_cb_hash, 0, sizeof(this->pixel_cb_hash));

    this->buffer_vertices = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::vertex, DYNAMIC_VB_SIZE);
    this->buffer_indices = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::index, DYNAMIC_IB_SIZE);
    this->buffer_vertex_cb[0] = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::constant, ::VS_CBUFFER0_SIZE);
    this->buffer_vertex_cb[1] = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::constant, ::VS_CBUFFER1_SIZE);
    this->buffer_pixel_cb[0] = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::constant, ::PS_CBUFFER0_SIZE);
    this->buffer_pixel_cb[1] = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::constant, ::PS_CBUFFER1_SIZE);
    this->buffer_pixel_cb[2] = std::make_shared<ff_dx::buffer>(ff::dxgi::buffer_type::constant, sizeof(::pixel_buffer_2));
}

void ff::internal::ui::render_device::create_state_objects()
{
    // Rasterized states
    {
        D3D11_RASTERIZER_DESC desc;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = false;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.MultisampleEnable = true;
        desc.AntialiasedLineEnable = false;

        desc.FillMode = D3D11_FILL_SOLID;
        desc.ScissorEnable = false;
        this->rasterizer_states[0] = ff_dx::get_object_cache().get_rasterize_state(desc);

        desc.FillMode = D3D11_FILL_WIREFRAME;
        desc.ScissorEnable = false;
        this->rasterizer_states[1] = ff_dx::get_object_cache().get_rasterize_state(desc);

        desc.FillMode = D3D11_FILL_SOLID;
        desc.ScissorEnable = true;
        this->rasterizer_state_scissor = ff_dx::get_object_cache().get_rasterize_state(desc);
    }

    // Blend states
    {
        static_assert(_countof(this->blend_states) == static_cast<size_t>(Noesis::BlendMode::Count));

        D3D11_BLEND_DESC desc;
        desc.AlphaToCoverageEnable = false;
        desc.IndependentBlendEnable = false;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        // Src
        desc.RenderTarget[0].BlendEnable = false;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::Src)] = ff_dx::get_object_cache().get_blend_state(desc);

        // SrcOver
        desc.RenderTarget[0].BlendEnable = true;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::SrcOver)] = ff_dx::get_object_cache().get_blend_state(desc);

        // SrcOver_Multiply
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::SrcOver_Multiply)] = ff_dx::get_object_cache().get_blend_state(desc);

        // SrcOver_Screen
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::SrcOver_Screen)] = ff_dx::get_object_cache().get_blend_state(desc);

        // SrcOver_Additive
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::SrcOver_Additive)] = ff_dx::get_object_cache().get_blend_state(desc);

        // SrcOver_Dual
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
        this->blend_states[static_cast<size_t>(Noesis::BlendMode::SrcOver_Dual)] = ff_dx::get_object_cache().get_blend_state(desc);

        // Color disabled
        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
        this->blend_state_no_color = ff_dx::get_object_cache().get_blend_state(desc);
    }

    // Depth states
    {
        static_assert(_countof(this->depth_stencil_states) == static_cast<size_t>(Noesis::StencilMode::Count));

        D3D11_DEPTH_STENCIL_DESC desc;
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_NEVER;
        desc.StencilReadMask = 0xff;
        desc.StencilWriteMask = 0xff;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
        desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

        // Disabled
        desc.StencilEnable = false;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Disabled)] = ff_dx::get_object_cache().get_depth_stencil_state(desc);

        // Equal_Keep
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Equal_Keep)] = ff_dx::get_object_cache().get_depth_stencil_state(desc);

        // Equal_Incr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Equal_Incr)] = ff_dx::get_object_cache().get_depth_stencil_state(desc);

        // Equal_Decr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Equal_Decr)] = ff_dx::get_object_cache().get_depth_stencil_state(desc);

        // Clear
        desc.StencilEnable = true;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        this->depth_stencil_states[static_cast<size_t>(Noesis::StencilMode::Clear)] = ff_dx::get_object_cache().get_depth_stencil_state(desc);
    }

    // Sampler states
    for (uint8_t minmag = 0; minmag < Noesis::MinMagFilter::Count; minmag++)
    {
        for (uint8_t mip = 0; mip < Noesis::MipFilter::Count; mip++)
        {
            for (uint8_t uv = 0; uv < Noesis::WrapMode::Count; uv++)
            {
                Noesis::SamplerState s = { { uv, minmag, mip } };
                this->sampler_states[s.v].params = s;
                this->sampler_states[s.v].state_.Reset();
            }
        }
    }
}

void ff::internal::ui::render_device::create_shaders()
{
    struct shader_t
    {
        std::string_view resource_name;
        uint32_t layout;
    };

    const shader_t pixel_shaders[] =
    {
        { "Noesis.RGBA_PS" },
        { "Noesis.Mask_PS" },
        { "Noesis.Clear_PS" },

        { "Noesis.PathSolid_PS" },
        { "Noesis.PathLinear_PS" },
        { "Noesis.PathRadial_PS" },
        { "Noesis.PathPattern_PS" },
        { "Noesis.PathPatternClamp_PS" },
        { "Noesis.PathPatternRepeat_PS" },
        { "Noesis.PathPatternMirrorU_PS" },
        { "Noesis.PathPatternMirrorV_PS" },
        { "Noesis.PathPatternMirror_PS" },

        { "Noesis.PathAASolid_PS" },
        { "Noesis.PathAALinear_PS" },
        { "Noesis.PathAARadial_PS" },
        { "Noesis.PathAAPattern_PS" },
        { "Noesis.PathAAPatternClamp_PS" },
        { "Noesis.PathAAPatternRepeat_PS" },
        { "Noesis.PathAAPatternMirrorU_PS" },
        { "Noesis.PathAAPatternMirrorV_PS" },
        { "Noesis.PathAAPatternMirror_PS" },

        { "Noesis.SDFSolid_PS" },
        { "Noesis.SDFLinear_PS" },
        { "Noesis.SDFRadial_PS" },
        { "Noesis.SDFPattern_PS" },
        { "Noesis.SDFPatternClamp_PS" },
        { "Noesis.SDFPatternRepeat_PS" },
        { "Noesis.SDFPatternMirrorU_PS" },
        { "Noesis.SDFPatternMirrorV_PS" },
        { "Noesis.SDFPatternMirror_PS" },

        { "Noesis.SDFLCDSolid_PS" },
        { "Noesis.SDFLCDLinear_PS" },
        { "Noesis.SDFLCDRadial_PS" },
        { "Noesis.SDFLCDPattern_PS" },
        { "Noesis.SDFLCDPatternClamp_PS" },
        { "Noesis.SDFLCDPatternRepeat_PS" },
        { "Noesis.SDFLCDPatternMirrorU_PS" },
        { "Noesis.SDFLCDPatternMirrorV_PS" },
        { "Noesis.SDFLCDPatternMirror_PS" },

        { "Noesis.OpacitySolid_PS" },
        { "Noesis.OpacityLinear_PS" },
        { "Noesis.OpacityRadial_PS" },
        { "Noesis.OpacityPattern_PS" },
        { "Noesis.OpacityPatternClamp_PS" },
        { "Noesis.OpacityPatternRepeat_PS" },
        { "Noesis.OpacityPatternMirrorU_PS" },
        { "Noesis.OpacityPatternMirrorV_PS" },
        { "Noesis.OpacityPatternMirror_PS" },

        { "Noesis.Upsample_PS" },
        { "Noesis.Downsample_PS" },

        { "Noesis.Shadow_PS" },
        { "Noesis.Blur_PS" }
    };

    const shader_t vertex_shaders[] =
    {
        { "Noesis.Pos_VS", 0 },
        { "Noesis.PosColor_VS", 1 },
        { "Noesis.PosTex0_VS", 2 },
        { "Noesis.PosTex0Rect_VS", 3 },
        { "Noesis.PosTex0RectTile_VS", 4 },
        { "Noesis.PosColorCoverage_VS", 5 },
        { "Noesis.PosTex0Coverage_VS", 6 },
        { "Noesis.PosTex0CoverageRect_VS", 7 },
        { "Noesis.PosTex0CoverageRectTile_VS", 8 },
        { "Noesis.PosColorTex1_SDF_VS", 9 },
        { "Noesis.PosTex0Tex1_SDF_VS", 10 },
        { "Noesis.PosTex0Tex1Rect_SDF_VS", 11 },
        { "Noesis.PosTex0Tex1RectTile_SDF_VS", 12 },
        { "Noesis.PosColorTex1_VS", 9 },
        { "Noesis.PosTex0Tex1_VS", 10 },
        { "Noesis.PosTex0Tex1Rect_VS", 11 },
        { "Noesis.PosTex0Tex1RectTile_VS", 12 },
        { "Noesis.PosColorTex0Tex1_VS", 13 },
        { "Noesis.PosTex0Tex1_Downsample_VS", 10 },
        { "Noesis.PosColorTex1Rect_VS", 14 },
        { "Noesis.PosColorTex0RectImagePos_VS", 15 }
    };

    const shader_t vertex_shaders_srgb[] =
    {
        { "Noesis.Pos_VS", 0 },
        { "Noesis.PosColor_SRGB_VS", 1 },
        { "Noesis.PosTex0_VS", 2 },
        { "Noesis.PosTex0Rect_VS", 3 },
        { "Noesis.PosTex0RectTile_VS", 4 },
        { "Noesis.PosColorCoverage_SRGB_VS", 5 },
        { "Noesis.PosTex0Coverage_VS", 6 },
        { "Noesis.PosTex0CoverageRect_VS", 7 },
        { "Noesis.PosTex0CoverageRectTile_VS", 8 },
        { "Noesis.PosColorTex1_SDF_SRGB_VS", 9 },
        { "Noesis.PosTex0Tex1_SDF_VS", 10 },
        { "Noesis.PosTex0Tex1Rect_SDF_VS", 11 },
        { "Noesis.PosTex0Tex1RectTile_SDF_VS", 12 },
        { "Noesis.PosColorTex1_SRGB_VS", 9 },
        { "Noesis.PosTex0Tex1_VS", 10 },
        { "Noesis.PosTex0Tex1Rect_VS", 11 },
        { "Noesis.PosTex0Tex1RectTile_VS", 12 },
        { "Noesis.PosColorTex0Tex1_SRGB_VS", 13 },
        { "Noesis.PosTex0Tex1_Downsample_VS", 10 },
        { "Noesis.PosColorTex1Rect_SRGB_VS", 14 },
        { "Noesis.PosColorTex0RectImagePos_SRGB_VS", 15 }
    };

    static_assert(_countof(vertex_shaders) == _countof(this->vertex_stages));
    static_assert(_countof(vertex_shaders) == _countof(vertex_shaders_srgb));
    static_assert(_countof(pixel_shaders) == _countof(this->pixel_shaders));

    for (uint32_t i = 0; i < _countof(vertex_shaders); i++)
    {
        const shader_t& shader = this->caps.linearRendering ? vertex_shaders_srgb[i] : vertex_shaders[i];
        this->vertex_stages[i].resource_name = shader.resource_name;
        this->vertex_stages[i].shader_.Reset();
        this->vertex_stages[i].layout_.Reset();
        this->vertex_stages[i].layout_index = shader.layout;
    }

    for (uint32_t i = 0; i < _countof(pixel_shaders); i++)
    {
        const shader_t& shader = pixel_shaders[i];
        this->pixel_shaders[i].resource_name = shader.resource_name;
        this->pixel_shaders[i].shader_.Reset();
    }

    this->quad_vs.resource_name = "Noesis.Quad_VS";
    this->quad_vs.shader_.Reset();

    this->resolve_ps[0].resource_name = "Noesis.Resolve2_PS";
    this->resolve_ps[0].shader_.Reset();

    this->resolve_ps[1].resource_name = "Noesis.Resolve4_PS";
    this->resolve_ps[1].shader_.Reset();

    this->resolve_ps[2].resource_name = "Noesis.Resolve8_PS";
    this->resolve_ps[2].shader_.Reset();

    this->resolve_ps[3].resource_name = "Noesis.Resolve16_PS";
    this->resolve_ps[3].shader_.Reset();

    std::memset(this->programs, 255, sizeof(this->programs));

    this->programs[Noesis::Shader::RGBA] = { 0, 0 };
    this->programs[Noesis::Shader::Mask] = { 0, 1 };
    this->programs[Noesis::Shader::Clear] = { 0, 2 };

    this->programs[Noesis::Shader::Path_Solid] = { 1, 3 };
    this->programs[Noesis::Shader::Path_Linear] = { 2, 4 };
    this->programs[Noesis::Shader::Path_Radial] = { 2, 5 };
    this->programs[Noesis::Shader::Path_Pattern] = { 2, 6 };
    this->programs[Noesis::Shader::Path_Pattern_Clamp] = { 3, 7 };
    this->programs[Noesis::Shader::Path_Pattern_Repeat] = { 4, 8 };
    this->programs[Noesis::Shader::Path_Pattern_MirrorU] = { 4, 9 };
    this->programs[Noesis::Shader::Path_Pattern_MirrorV] = { 4, 10 };
    this->programs[Noesis::Shader::Path_Pattern_Mirror] = { 4, 11 };

    this->programs[Noesis::Shader::Path_AA_Solid] = { 5, 12 };
    this->programs[Noesis::Shader::Path_AA_Linear] = { 6, 13 };
    this->programs[Noesis::Shader::Path_AA_Radial] = { 6, 14 };
    this->programs[Noesis::Shader::Path_AA_Pattern] = { 6, 15 };
    this->programs[Noesis::Shader::Path_AA_Pattern_Clamp] = { 7, 16 };
    this->programs[Noesis::Shader::Path_AA_Pattern_Repeat] = { 8, 17 };
    this->programs[Noesis::Shader::Path_AA_Pattern_MirrorU] = { 8, 18 };
    this->programs[Noesis::Shader::Path_AA_Pattern_MirrorV] = { 8, 19 };
    this->programs[Noesis::Shader::Path_AA_Pattern_Mirror] = { 8, 20 };

    this->programs[Noesis::Shader::SDF_Solid] = { 9, 21 };
    this->programs[Noesis::Shader::SDF_Linear] = { 10, 22 };
    this->programs[Noesis::Shader::SDF_Radial] = { 10, 23 };
    this->programs[Noesis::Shader::SDF_Pattern] = { 10, 24 };
    this->programs[Noesis::Shader::SDF_Pattern_Clamp] = { 11, 25 };
    this->programs[Noesis::Shader::SDF_Pattern_Repeat] = { 12, 26 };
    this->programs[Noesis::Shader::SDF_Pattern_MirrorU] = { 12, 27 };
    this->programs[Noesis::Shader::SDF_Pattern_MirrorV] = { 12, 28 };
    this->programs[Noesis::Shader::SDF_Pattern_Mirror] = { 12, 29 };

    this->programs[Noesis::Shader::SDF_LCD_Solid] = { 9, 30 };
    this->programs[Noesis::Shader::SDF_LCD_Linear] = { 10, 31 };
    this->programs[Noesis::Shader::SDF_LCD_Radial] = { 10, 32 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern] = { 10, 33 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern_Clamp] = { 11, 34 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern_Repeat] = { 12, 35 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern_MirrorU] = { 12, 36 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern_MirrorV] = { 12, 37 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern_Mirror] = { 12, 38 };

    this->programs[Noesis::Shader::Opacity_Solid] = { 13, 39 };
    this->programs[Noesis::Shader::Opacity_Linear] = { 14, 40 };
    this->programs[Noesis::Shader::Opacity_Radial] = { 14, 41 };
    this->programs[Noesis::Shader::Opacity_Pattern] = { 14, 42 };
    this->programs[Noesis::Shader::Opacity_Pattern_Clamp] = { 15, 43 };
    this->programs[Noesis::Shader::Opacity_Pattern_Repeat] = { 16, 44 };
    this->programs[Noesis::Shader::Opacity_Pattern_MirrorU] = { 16, 45 };
    this->programs[Noesis::Shader::Opacity_Pattern_MirrorV] = { 16, 46 };
    this->programs[Noesis::Shader::Opacity_Pattern_Mirror] = { 16, 47 };

    this->programs[Noesis::Shader::Upsample] = { 17, 48 };
    this->programs[Noesis::Shader::Downsample] = { 18, 49 };

    this->programs[Noesis::Shader::Shadow] = { 19, 50 };
    this->programs[Noesis::Shader::Blur] = { 13, 51 };
    this->programs[Noesis::Shader::Custom_Effect] = { 20, 0 };
}

void ff::internal::ui::render_device::clear_textures()
{
    ID3D11ShaderResourceView* textures[static_cast<size_t>(texture_slot_t::Count)] = { nullptr };
    ff_dx::get_device_state().set_resources_ps(textures, 0, _countof(textures));
}

void ff::internal::ui::render_device::set_shaders(const Noesis::Batch& batch)
{
    const vertex_and_pixel_program_t& program = this->programs[batch.shader.v];
    vertex_shader_and_layout_t& vertex = this->vertex_stages[program.vertex_shader_index];
    ff_dx::get_device_state().set_layout_ia(vertex.layout());
    ff_dx::get_device_state().set_vs(vertex.shader());

    pixel_shader_t& pixel = this->pixel_shaders[program.pixel_shader_index];
    ff_dx::get_device_state().set_ps(batch.pixelShader
        ? reinterpret_cast<ID3D11PixelShader*>(batch.pixelShader)
        : pixel.shader());
}

void ff::internal::ui::render_device::set_buffers(const Noesis::Batch& batch)
{
    // Indices
    ff_dx::get_device_state().set_index_ia(this->buffer_indices->dx_buffer(), DXGI_FORMAT_R16_UINT, 0);

    // Vertices
    const vertex_and_pixel_program_t& program = this->programs[batch.shader.v];
    unsigned int stride = ::LAYOUT_FORMATS_AND_STRIDE[this->vertex_stages[program.vertex_shader_index].layout_index].second;
    ff_dx::get_device_state().set_vertex_ia(this->buffer_vertices->dx_buffer(), stride, batch.vertexOffset);

    // Vertex Shader Constant Buffers
    static_assert(_countof(this->buffer_vertex_cb) == _countof(Noesis::Batch::vertexUniforms));
    static_assert(_countof(this->vertex_cb_hash) == _countof(Noesis::Batch::vertexUniforms));

    for (uint32_t i = 0; i < _countof(this->buffer_vertex_cb); i++)
    {
        if (batch.vertexUniforms[i].numDwords > 0)
        {
            if (this->vertex_cb_hash[i] != batch.vertexUniforms[i].hash)
            {
                uint32_t size = batch.vertexUniforms[i].numDwords * sizeof(uint32_t);
                void* ptr = this->buffer_vertex_cb[i]->map(ff_dx::get_device_state(), size);
                std::memcpy(ptr, batch.vertexUniforms[i].values, size);
                this->buffer_vertex_cb[i]->unmap();

                this->vertex_cb_hash[i] = batch.vertexUniforms[i].hash;
            }
        }
    }

    // Pixel Shader Constant Buffers
    static_assert(_countof(this->buffer_pixel_cb) == _countof(Noesis::Batch::pixelUniforms) + 1);
    static_assert(_countof(this->pixel_cb_hash) == _countof(Noesis::Batch::pixelUniforms) + 1);

    for (uint32_t i = 0; i < _countof(this->buffer_pixel_cb) - 1; i++)
    {
        if (batch.pixelUniforms[i].numDwords > 0)
        {
            if (this->pixel_cb_hash[i] != batch.pixelUniforms[i].hash)
            {
                uint32_t size = batch.pixelUniforms[i].numDwords * sizeof(uint32_t);
                void* ptr = this->buffer_pixel_cb[i]->map(ff_dx::get_device_state(), size);
                std::memcpy(ptr, batch.pixelUniforms[i].values, size);
                this->buffer_pixel_cb[i]->unmap();

                this->pixel_cb_hash[i] = batch.pixelUniforms[i].hash;
            }
        }
    }

    // Palette constant buffer
    {
        ff::internal::ui::texture* pattern_texture = ff::internal::ui::texture::get(batch.pattern);

        ::pixel_buffer_2 pb2{};
        pb2.pattern_size = pattern_texture ? pattern_texture->internal_texture()->size().cast<float>() : ff::point_float(0, 0);
        pb2.pattern_inverse_size = pattern_texture ? ff::point_float(1, 1) / pb2.pattern_size : ff::point_float(0, 0);
        pb2.palette_row = static_cast<unsigned int>(ff::ui::global_palette() ? ff::ui::global_palette()->current_row() : 0);
        uint32_t hash = static_cast<uint32_t>(ff::stable_hash_func(pb2));

        if (hash != this->pixel_cb_hash[2])
        {
            void* ptr = this->buffer_pixel_cb[2]->map(ff_dx::get_device_state(), sizeof(pb2));
            std::memcpy(ptr, &pb2, sizeof(pb2));
            this->buffer_pixel_cb[2]->unmap();

            this->pixel_cb_hash[2] = hash;
        }
    }
}

void ff::internal::ui::render_device::set_render_state(const Noesis::Batch& batch)
{
    auto f = batch.renderState.f;

    assert(f.wireframe < _countof(this->rasterizer_states));
    ID3D11RasterizerState* rasterizer = this->rasterizer_states[f.wireframe].Get();
    ff_dx::get_device_state().set_raster(rasterizer);

    assert(f.blendMode < _countof(this->blend_states));
    ID3D11BlendState* blend = f.colorEnable ? this->blend_states[f.blendMode].Get() : this->blend_state_no_color.Get();
    ff_dx::get_device_state().set_blend(blend, ff::color::none(), 0xffffffff);

    assert(f.stencilMode < _countof(this->depth_stencil_states));
    ID3D11DepthStencilState* stencil = this->depth_stencil_states[f.stencilMode].Get();
    ff_dx::get_device_state().set_depth(stencil, batch.stencilRef);
}

void ff::internal::ui::render_device::set_textures(const Noesis::Batch& batch)
{
    if (batch.pattern)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.pattern);
        bool palette = ff::dxgi::palette_format(t->internal_texture()->format());
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11ShaderResourceView* palette_view = (palette && ff::ui::global_palette()) ? ff::ui::global_palette()->data()->texture()->view() : nullptr;
#ifdef _DEBUG
        ID3D11ShaderResourceView* empty_view = this->empty_texture_rgb->view();
        ID3D11ShaderResourceView* empty_palette_view = this->empty_texture_palette->view();
#else
        ID3D11ShaderResourceView* empty_view = nullptr;
        ID3D11ShaderResourceView* empty_palette_view = nullptr;
#endif
        ID3D11ShaderResourceView* views[3] = { !palette ? view : empty_view, palette ? view : empty_palette_view, palette_view };
        ID3D11SamplerState* sampler = this->sampler_states[batch.patternSampler.v].state();
        ff_dx::get_device_state().set_resources_ps(&views[0], static_cast<size_t>(texture_slot_t::Pattern), 1);
        ff_dx::get_device_state().set_resources_ps(&views[1], static_cast<size_t>(texture_slot_t::PaletteImage), 1);
        ff_dx::get_device_state().set_resources_ps(&views[2], static_cast<size_t>(texture_slot_t::Palette), 1);
        ff_dx::get_device_state().set_samplers_ps(&sampler, static_cast<size_t>(texture_slot_t::Pattern), 1);
    }

    if (batch.ramps)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.ramps);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_states[batch.rampsSampler.v].state();
        ff_dx::get_device_state().set_resources_ps(&view, static_cast<size_t>(texture_slot_t::Ramps), 1);
        ff_dx::get_device_state().set_samplers_ps(&sampler, static_cast<size_t>(texture_slot_t::Ramps), 1);
    }

    if (batch.image)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.image);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_states[batch.imageSampler.v].state();
        ff_dx::get_device_state().set_resources_ps(&view, static_cast<size_t>(texture_slot_t::Image), 1);
        ff_dx::get_device_state().set_samplers_ps(&sampler, static_cast<size_t>(texture_slot_t::Image), 1);
    }

    if (batch.glyphs)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.glyphs);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_states[batch.glyphsSampler.v].state();
        ff_dx::get_device_state().set_resources_ps(&view, static_cast<size_t>(texture_slot_t::Glyphs), 1);
        ff_dx::get_device_state().set_samplers_ps(&sampler, static_cast<size_t>(texture_slot_t::Glyphs), 1);
    }

    if (batch.shadow)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.shadow);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_states[batch.shadowSampler.v].state();
        ff_dx::get_device_state().set_resources_ps(&view, static_cast<size_t>(texture_slot_t::Shadow), 1);
        ff_dx::get_device_state().set_samplers_ps(&sampler, static_cast<size_t>(texture_slot_t::Shadow), 1);
    }
}

ID3D11VertexShader* ff::internal::ui::render_device::vertex_shader_t::shader()
{
    if (!this->shader_)
    {
        this->shader_ = ff_dx::get_object_cache().get_vertex_shader(this->resource_name);
    }

    return this->shader_.Get();
}

ID3D11InputLayout* ff::internal::ui::render_device::vertex_shader_and_layout_t::layout()
{
    if (!this->layout_)
    {
        this->layout_ = ff::internal::ui::render_device::create_layout(::LAYOUT_FORMATS_AND_STRIDE[this->layout_index].first, this->resource_name);
    }

    return this->layout_.Get();
}

ID3D11PixelShader* ff::internal::ui::render_device::pixel_shader_t::shader()
{
    if (!this->shader_)
    {
        this->shader_ = ff_dx::get_object_cache().get_pixel_shader(this->resource_name);
    }

    return this->shader_.Get();
}

ID3D11SamplerState* ff::internal::ui::render_device::sampler_state_t::state()
{
    if (!this->state_)
    {
        D3D11_SAMPLER_DESC desc{};
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = -D3D11_FLOAT32_MAX;
        desc.MipLODBias = -0.75f;

        ::set_filter(Noesis::MinMagFilter::Enum(this->params.f.minmagFilter), Noesis::MipFilter::Enum(this->params.f.mipFilter), desc);
        ::set_address(Noesis::WrapMode::Enum(this->params.f.wrapMode), ff_dx::feature_level(), desc);

        this->state_ = ff_dx::get_object_cache().get_sampler_state(desc);
    }

    return this->state_.Get();
}

#endif
