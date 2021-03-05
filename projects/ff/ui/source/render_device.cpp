#include "pch.h"
#include "render_device.h"
#include "render_target.h"
#include "texture.h"
#include "ui.h"

namespace
{
    struct texture_dimensions_t
    {
        ff::point_float image_size;
        ff::point_float image_inverse_size;
        ff::point_float pattern_size;
        ff::point_float pattern_inverse_size;
        unsigned int palette_row;
        unsigned int padding[3];
    };
}

static const size_t PREALLOCATED_DYNAMIC_PAGES = 1;
static const size_t VS_CBUFFER_SIZE = 16 * sizeof(float); // projectionMtx
static const size_t PS_CBUFFER_SIZE = 12 * sizeof(float); // rgba | radialGrad opacity | opacity
static const size_t PS_EFFECTS_SIZE = 16 * sizeof(float);
static const size_t TEX_DIMENSIONS_SIZE = sizeof(texture_dimensions_t);
static const uint32_t VFPos = 0;
static const uint32_t VFColor = 1;
static const uint32_t VFTex0 = 2;
static const uint32_t VFTex1 = 4;
static const uint32_t VFTex2 = 8;
static const uint32_t VFCoverage = 16;

static const Noesis::Pair<uint32_t, uint32_t> LAYOUT_FORMATS_AND_STRIDE[] =
{
    { VFPos, 8 },
    { VFPos | VFColor, 12 },
    { VFPos | VFTex0, 16 },
    { VFPos | VFColor | VFCoverage, 16 },
    { VFPos | VFTex0 | VFCoverage, 20 },
    { VFPos | VFColor | VFTex1, 20 },
    { VFPos | VFTex0 | VFTex1, 24 },
    { VFPos | VFColor | VFTex1 | VFTex2, 28 },
    { VFPos | VFTex0 | VFTex1 | VFTex2, 32 },
};

static D3D11_FILTER ToD3D(Noesis::MinMagFilter::Enum min_mag_filter, Noesis::MipFilter::Enum mip_filter)
{
    switch (min_mag_filter)
    {
        default:
        case Noesis::MinMagFilter::Nearest:
            switch (mip_filter)
            {
                default:
                case Noesis::MipFilter::Disabled:
                case Noesis::MipFilter::Nearest:
                    return D3D11_FILTER_MIN_MAG_MIP_POINT;

                case Noesis::MipFilter::Linear:
                    return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            }
            break;

        case Noesis::MinMagFilter::Linear:
            switch (mip_filter)
            {
                default:
                case Noesis::MipFilter::Disabled:
                case Noesis::MipFilter::Nearest:
                    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

                case Noesis::MipFilter::Linear:
                    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            }
            break;
    }
}

static void ToD3D(Noesis::WrapMode::Enum mode, D3D11_TEXTURE_ADDRESS_MODE& address_u, D3D11_TEXTURE_ADDRESS_MODE& address_v)
{
    switch (mode)
    {
        default:
        case Noesis::WrapMode::ClampToEdge:
            address_u = D3D11_TEXTURE_ADDRESS_CLAMP;
            address_v = D3D11_TEXTURE_ADDRESS_CLAMP;
            break;

        case Noesis::WrapMode::ClampToZero:
            address_u = D3D11_TEXTURE_ADDRESS_BORDER;
            address_v = D3D11_TEXTURE_ADDRESS_BORDER;
            break;

        case Noesis::WrapMode::Repeat:
            address_u = D3D11_TEXTURE_ADDRESS_WRAP;
            address_v = D3D11_TEXTURE_ADDRESS_WRAP;
            break;

        case Noesis::WrapMode::MirrorU:
            address_u = D3D11_TEXTURE_ADDRESS_MIRROR;
            address_v = D3D11_TEXTURE_ADDRESS_WRAP;
            break;

        case Noesis::WrapMode::MirrorV:
            address_u = D3D11_TEXTURE_ADDRESS_WRAP;
            address_v = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;

        case Noesis::WrapMode::Mirror:
            address_u = D3D11_TEXTURE_ADDRESS_MIRROR;
            address_v = D3D11_TEXTURE_ADDRESS_MIRROR;
            break;
    }
}

ff::internal::ui::render_device::render_device(bool srgb)
    : vertex_cb_hash(0)
    , pixel_cb_hash(0)
    , effect_cb_hash(0)
    , texture_dimensions_cb_hash(0)
    , null_textures{}
{
    ff::internal::graphics::add_child(this);

#ifdef _DEBUG
    this->empty_texture_rgb = std::make_shared<ff::dx11_texture>(ff::point_int(1, 1));
    this->empty_texture_palette = std::make_shared<ff::dx11_texture>(ff::point_int(1, 1), ff::internal::PALETTE_INDEX_FORMAT);
#endif

    this->caps.centerPixelOffset = 0;
    this->caps.linearRendering = srgb;
    this->caps.subpixelRendering = false;

    this->create_buffers();
    this->create_state_objects();
    this->create_shaders();
}

ff::internal::ui::render_device::~render_device()
{
    ff::internal::graphics::remove_child(this);
}

const Noesis::DeviceCaps& ff::internal::ui::render_device::GetCaps() const
{
    return this->caps;
}

Noesis::Ptr<Noesis::RenderTarget> ff::internal::ui::render_device::CreateRenderTarget(const char* label, uint32_t width, uint32_t height, uint32_t sample_count)
{
    std::string_view name(label ? label : "");
    return *new ff::internal::ui::render_target(static_cast<size_t>(width), static_cast<size_t>(height), static_cast<size_t>(sample_count), this->caps.linearRendering, name);
}

Noesis::Ptr<Noesis::RenderTarget> ff::internal::ui::render_device::CloneRenderTarget(const char* label, Noesis::RenderTarget* surface)
{
    std::string_view name(label ? label : "");
    return ff::internal::ui::render_target::get(surface)->clone(name);
}

Noesis::Ptr<Noesis::Texture> ff::internal::ui::render_device::CreateTexture(const char* label, uint32_t width, uint32_t height, uint32_t mip_count, Noesis::TextureFormat::Enum format, const void** data)
{
    std::string_view name(label ? label : "");
    DXGI_FORMAT format2 = (format == Noesis::TextureFormat::R8) ? DXGI_FORMAT_R8_UNORM : (this->caps.linearRendering ? ff::internal::DEFAULT_FORMAT_SRGB : ff::internal::DEFAULT_FORMAT);
    std::shared_ptr<ff::dx11_texture> texture;

    if (data == nullptr)
    {
        ff::point_int size = ff::point_t<uint32_t>(width, height).cast<int>();

        texture = std::make_shared<ff::dx11_texture>(size, format2, mip_count);
    }
    else
    {
        const uint32_t bpp = (format == DXGI_FORMAT_R8_UNORM) ? 1 : 4;

        DirectX::ScratchImage scratch;
        if (FAILED(scratch.Initialize2D(format2, width, height, 1, mip_count)))
        {
            assert(false);
            return nullptr;
        }

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

        texture = std::make_shared<ff::dx11_texture>(std::make_shared<DirectX::ScratchImage>(std::move(scratch)));
    }

    return *new ff::internal::ui::texture(texture, name);
}

void ff::internal::ui::render_device::UpdateTexture(Noesis::Texture* texture, uint32_t level, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void* data)
{
    ff::dx11_texture* texture2 = ff::internal::ui::texture::get(texture)->internal_texture().get();
    texture2->update(0, static_cast<size_t>(level), ff::rect_t<uint32_t>(x, y, x + width, y + height).cast<int>(), data, texture2->format());
}

void ff::internal::ui::render_device::BeginRender(bool offscreen)
{
    std::array<ID3D11Buffer*, 2> buffer_vs =
    {
        this->buffer_vertex_cb->buffer(),
        this->buffer_texture_dimensions_cb->buffer(),
    };

    std::array<ID3D11Buffer*, 3> buffer_ps =
    {
        this->buffer_pixel_cb->buffer(),
        this->buffer_texture_dimensions_cb->buffer(),
        this->buffer_effect_cb->buffer(),
    };

    ff::graphics::dx11_device_state().set_constants_vs(buffer_vs.data(), 0, buffer_vs.size());
    ff::graphics::dx11_device_state().set_constants_ps(buffer_ps.data(), 0, buffer_ps.size());
    ff::graphics::dx11_device_state().set_topology_ia(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ff::graphics::dx11_device_state().set_gs(nullptr);
}

void ff::internal::ui::render_device::SetRenderTarget(Noesis::RenderTarget* surface)
{
    this->clear_textures();

    ff::internal::ui::render_target* surface2 = ff::internal::ui::render_target::get(surface);
    ff::point_float size = surface2->msaa_texture()->size().cast<float>();
    ID3D11RenderTargetView* view = surface2->msaa_target()->view();
    ff::graphics::dx11_device_state().set_targets(&view, 1, surface2->depth()->view());

    D3D11_VIEWPORT viewport{};
    viewport.Width = size.x;
    viewport.Height = size.y;
    viewport.MaxDepth = 1.0f;

    ff::graphics::dx11_device_state().set_viewports(&viewport, 1);
}

void ff::internal::ui::render_device::BeginTile(const Noesis::Tile& tile, uint32_t surface_width, uint32_t surface_height)
{
    uint32_t x = tile.x;
    uint32_t y = surface_height - (tile.y + tile.height);

    D3D11_RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + tile.width;
    rect.bottom = y + tile.height;

    ff::graphics::dx11_device_state().set_scissors(&rect, 1);

    this->clear_render_target();
}

void ff::internal::ui::render_device::EndTile()
{}

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

        ff::graphics::dx11_device_state().set_layout_ia(nullptr);
        ff::graphics::dx11_device_state().set_vs(this->quad_vs.shader());
        ff::graphics::dx11_device_state().set_ps(this->resolve_ps[index_ps].shader());

        ff::graphics::dx11_device_state().set_raster(this->rasterizer_states[2].Get());
        ff::graphics::dx11_device_state().set_blend(this->blend_states[0].Get(), ff::color::none(), 0xffffffff);
        ff::graphics::dx11_device_state().set_depth(this->depth_stencil_states[0].Get(), 0);

        this->clear_textures();
        ID3D11RenderTargetView* view = surface2->resolved_target()->view();
        ff::graphics::dx11_device_state().set_targets(&view, 1, nullptr);

        ID3D11ShaderResourceView* resourceView = surface2->msaa_texture()->view();
        ff::graphics::dx11_device_state().set_resources_ps(&resourceView, 0, 1);

        ff::point_int size = surface2->resolved_texture()->size();

        for (uint32_t i = 0; i < tile_count; i++)
        {
            const Noesis::Tile& tile = tiles[i];

            D3D11_RECT rect;
            rect.left = tile.x;
            rect.top = size.y - (tile.y + tile.height);
            rect.right = tile.x + tile.width;
            rect.bottom = size.y - tile.y;
            ff::graphics::dx11_device_state().set_scissors(&rect, 1);

            ff::graphics::dx11_device_state().draw(3, 0);
        }
    }
}

void ff::internal::ui::render_device::EndRender()
{}

void* ff::internal::ui::render_device::MapVertices(uint32_t bytes)
{
    return this->buffer_vertices->map(bytes);
}

void ff::internal::ui::render_device::UnmapVertices()
{
    this->buffer_vertices->unmap();
}

void* ff::internal::ui::render_device::MapIndices(uint32_t bytes)
{
    return this->buffer_indices->map(bytes);
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
    assert(program.pixel_shader_index != -1 && program.pixel_shader_index < _countof(this->pixel_stages));

    this->set_shaders(batch);
    this->set_buffers(batch);
    this->set_render_state(batch);
    this->set_textures(batch);

    ff::graphics::dx11_device_state().draw_indexed(batch.numIndices, batch.startIndex, 0);
    ff::graphics::dx11_device_state().set_resources_ps(this->null_textures.data(), 0, this->null_textures.size());
}

bool ff::internal::ui::render_device::reset()
{
    this->vertex_cb_hash = 0;
    this->pixel_cb_hash = 0;
    this->effect_cb_hash = 0;
    this->texture_dimensions_cb_hash = 0;

    this->create_buffers();
    this->create_state_objects();
    this->create_shaders();

    return true;
}

Microsoft::WRL::ComPtr<ID3D11InputLayout> ff::internal::ui::render_device::create_layout(uint32_t format, std::string_view vertex_resource_name)
{
    D3D11_INPUT_ELEMENT_DESC descs[5];
    uint32_t element = 0;

    descs[element].SemanticIndex = 0;
    descs[element].InputSlot = 0;
    descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
    descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    descs[element].InstanceDataStepRate = 0;
    descs[element].SemanticName = "POSITION";
    descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
    element++;

    if (format & VFColor)
    {
        descs[element].SemanticIndex = 0;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "COLOR";
        descs[element].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        element++;
    }

    if (format & VFTex0)
    {
        descs[element].SemanticIndex = 0;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        element++;
    }

    if (format & VFTex1)
    {
        descs[element].SemanticIndex = 1;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32G32_FLOAT;
        element++;
    }

    if (format & VFTex2)
    {
        descs[element].SemanticIndex = 2;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R16G16B16A16_UNORM;
        element++;
    }

    if (format & VFCoverage)
    {
        descs[element].SemanticIndex = 3;
        descs[element].InputSlot = 0;
        descs[element].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        descs[element].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        descs[element].InstanceDataStepRate = 0;
        descs[element].SemanticName = "TEXCOORD";
        descs[element].Format = DXGI_FORMAT_R32_FLOAT;
        element++;
    }

    return ff::graphics::dx11_object_cache().get_input_layout(vertex_resource_name, descs, element);
}

void ff::internal::ui::render_device::create_buffers()
{
    this->buffer_vertices = std::make_shared<ff::dx11_buffer>(D3D11_BIND_VERTEX_BUFFER, DYNAMIC_VB_SIZE);
    this->buffer_indices = std::make_shared<ff::dx11_buffer>(D3D11_BIND_INDEX_BUFFER, DYNAMIC_IB_SIZE);
    this->buffer_vertex_cb = std::make_shared<ff::dx11_buffer>(D3D11_BIND_CONSTANT_BUFFER, ::VS_CBUFFER_SIZE);
    this->buffer_pixel_cb = std::make_shared<ff::dx11_buffer>(D3D11_BIND_CONSTANT_BUFFER, ::PS_CBUFFER_SIZE);
    this->buffer_effect_cb = std::make_shared<ff::dx11_buffer>(D3D11_BIND_CONSTANT_BUFFER, ::PS_EFFECTS_SIZE);
    this->buffer_texture_dimensions_cb = std::make_shared<ff::dx11_buffer>(D3D11_BIND_CONSTANT_BUFFER, ::TEX_DIMENSIONS_SIZE);
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
        this->rasterizer_states[0] = ff::graphics::dx11_object_cache().get_rasterize_state(desc);

        desc.FillMode = D3D11_FILL_WIREFRAME;
        desc.ScissorEnable = false;
        this->rasterizer_states[1] = ff::graphics::dx11_object_cache().get_rasterize_state(desc);

        desc.FillMode = D3D11_FILL_SOLID;
        desc.ScissorEnable = true;
        this->rasterizer_states[2] = ff::graphics::dx11_object_cache().get_rasterize_state(desc);

        desc.FillMode = D3D11_FILL_WIREFRAME;
        desc.ScissorEnable = true;
        this->rasterizer_states[3] = ff::graphics::dx11_object_cache().get_rasterize_state(desc);
    }

    // Blend states
    {
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
        this->blend_states[0] = ff::graphics::dx11_object_cache().get_blend_state(desc);

        // SrcOver
        desc.RenderTarget[0].BlendEnable = true;
        this->blend_states[1] = ff::graphics::dx11_object_cache().get_blend_state(desc);

        // SrcOver_Dual
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_COLOR;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC1_ALPHA;
        this->blend_states[2] = ff::graphics::dx11_object_cache().get_blend_state(desc);

        // Color disabled
        desc.RenderTarget[0].BlendEnable = false;
        desc.RenderTarget[0].RenderTargetWriteMask = 0;
        this->blend_states[3] = ff::graphics::dx11_object_cache().get_blend_state(desc);
    }

    // Depth states
    {
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
        this->depth_stencil_states[0] = ff::graphics::dx11_object_cache().get_depth_stencil_state(desc);

        // Equal_Keep
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        this->depth_stencil_states[1] = ff::graphics::dx11_object_cache().get_depth_stencil_state(desc);

        // Equal_Incr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
        this->depth_stencil_states[2] = ff::graphics::dx11_object_cache().get_depth_stencil_state(desc);

        // Equal_Decr
        desc.StencilEnable = true;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
        this->depth_stencil_states[3] = ff::graphics::dx11_object_cache().get_depth_stencil_state(desc);

        // Zero
        desc.StencilEnable = true;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_ZERO;
        this->depth_stencil_states[4] = ff::graphics::dx11_object_cache().get_depth_stencil_state(desc);
    }

    // Sampler states
    for (uint8_t minmag = 0; minmag < Noesis::MinMagFilter::Count; minmag++)
    {
        for (uint8_t mip = 0; mip < Noesis::MipFilter::Count; mip++)
        {
            for (uint8_t uv = 0; uv < Noesis::WrapMode::Count; uv++)
            {
                Noesis::SamplerState s = { { uv, minmag, mip } };
                this->sampler_stages[s.v].params = s;
                this->sampler_stages[s.v].state_.Reset();
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

        { "Noesis.PathSolid_PS" },
        { "Noesis.PathLinear_PS" },
        { "Noesis.PathRadial_PS" },
        { "Noesis.PathPattern_PS" },

        { "Noesis.PathAASolid_PS" },
        { "Noesis.PathAALinear_PS" },
        { "Noesis.PathAARadial_PS" },
        { "Noesis.PathAAPattern_PS" },

        { "Noesis.SDFSolid_PS" },
        { "Noesis.SDFLinear_PS" },
        { "Noesis.SDFRadial_PS" },
        { "Noesis.SDFPattern_PS" },

        { "Noesis.SDFLCDSolid_PS" },
        { "Noesis.SDFLCDLinear_PS" },
        { "Noesis.SDFLCDRadial_PS" },
        { "Noesis.SDFLCDPattern_PS" },

        { "Noesis.ImageOpacitySolid_PS" },
        { "Noesis.ImageOpacityLinear_PS" },
        { "Noesis.ImageOpacityRadial_PS" },
        { "Noesis.ImageOpacityPattern_PS" },

        { "Noesis.ImageShadow35V_PS" },
        { "Noesis.ImageShadow63V_PS" },
        { "Noesis.ImageShadow127V_PS" },

        { "Noesis.ImageShadow35HSolid_PS" },
        { "Noesis.ImageShadow35HLinear_PS" },
        { "Noesis.ImageShadow35HRadial_PS" },
        { "Noesis.ImageShadow35HPattern_PS" },

        { "Noesis.ImageShadow63HSolid_PS" },
        { "Noesis.ImageShadow63HLinear_PS" },
        { "Noesis.ImageShadow63HRadial_PS" },
        { "Noesis.ImageShadow63HPattern_PS" },

        { "Noesis.ImageShadow127HSolid_PS" },
        { "Noesis.ImageShadow127HLinear_PS" },
        { "Noesis.ImageShadow127HRadial_PS" },
        { "Noesis.ImageShadow127HPattern_PS" },

        { "Noesis.ImageBlur35V_PS" },
        { "Noesis.ImageBlur63V_PS" },
        { "Noesis.ImageBlur127V_PS" },

        { "Noesis.ImageBlur35HSolid_PS" },
        { "Noesis.ImageBlur35HLinear_PS" },
        { "Noesis.ImageBlur35HRadial_PS" },
        { "Noesis.ImageBlur35HPattern_PS" },

        { "Noesis.ImageBlur63HSolid_PS" },
        { "Noesis.ImageBlur63HLinear_PS" },
        { "Noesis.ImageBlur63HRadial_PS" },
        { "Noesis.ImageBlur63HPattern_PS" },

        { "Noesis.ImageBlur127HSolid_PS" },
        { "Noesis.ImageBlur127HLinear_PS" },
        { "Noesis.ImageBlur127HRadial_PS" },
        { "Noesis.ImageBlur127HPattern_PS" },
    };

    const shader_t vertex_shaders[] =
    {
        { "Noesis.Pos_VS", 0 },
        { "Noesis.PosColor_VS", 1 },
        { "Noesis.PosTex0_VS", 2 },
        { "Noesis.PosColorCoverage_VS", 3 },
        { "Noesis.PosTex0Coverage_VS", 4 },
        { "Noesis.PosColorTex1_VS", 5 },
        { "Noesis.PosTex0Tex1_VS", 6 },
        { "Noesis.PosColorTex1_SDF_VS", 5 },
        { "Noesis.PosTex0Tex1_SDF_VS", 6 },
        { "Noesis.PosColorTex1Tex2_VS", 7 },
        { "Noesis.PosTex0Tex1Tex2_VS", 8 },
    };

    static_assert(_countof(vertex_shaders) == _countof(this->vertex_stages));
    static_assert(_countof(pixel_shaders) == _countof(this->pixel_stages));
    static_assert(_countof(pixel_shaders) == _countof(this->programs));

    for (uint32_t i = 0; i < _countof(vertex_shaders); i++)
    {
        const shader_t& shader = vertex_shaders[i];
        this->vertex_stages[i].resource_name = shader.resource_name;
        this->vertex_stages[i].shader_.Reset();
        this->vertex_stages[i].layout_.Reset();
        this->vertex_stages[i].layout_index = shader.layout;
    }

    for (uint32_t i = 0; i < _countof(pixel_shaders); i++)
    {
        const shader_t& shader = pixel_shaders[i];
        this->pixel_stages[i].resource_name = shader.resource_name;
        this->pixel_stages[i].shader_.Reset();
    }

    this->resolve_ps[0].resource_name = "Noesis.Resolve2_PS";
    this->resolve_ps[0].shader_.Reset();

    this->resolve_ps[1].resource_name = "Noesis.Resolve4_PS";
    this->resolve_ps[1].shader_.Reset();

    this->resolve_ps[2].resource_name = "Noesis.Resolve8_PS";
    this->resolve_ps[2].shader_.Reset();

    this->resolve_ps[3].resource_name = "Noesis.Resolve16_PS";
    this->resolve_ps[3].shader_.Reset();

    this->clear_ps.resource_name = "Noesis.Clear_PS";
    this->clear_ps.shader_.Reset();

    this->quad_vs.resource_name = "Noesis.Quad_VS";
    this->quad_vs.shader_.Reset();

    std::memset(this->programs, 255, sizeof(this->programs));

    this->programs[Noesis::Shader::RGBA] = { 0, 0 };
    this->programs[Noesis::Shader::Mask] = { 0, 1 };

    this->programs[Noesis::Shader::Path_Solid] = { 1, 2 };
    this->programs[Noesis::Shader::Path_Linear] = { 2, 3 };
    this->programs[Noesis::Shader::Path_Radial] = { 2, 4 };
    this->programs[Noesis::Shader::Path_Pattern] = { 2, 5 };

    this->programs[Noesis::Shader::PathAA_Solid] = { 3, 6 };
    this->programs[Noesis::Shader::PathAA_Linear] = { 4, 7 };
    this->programs[Noesis::Shader::PathAA_Radial] = { 4, 8 };
    this->programs[Noesis::Shader::PathAA_Pattern] = { 4, 9 };

    this->programs[Noesis::Shader::SDF_Solid] = { 7, 10 };
    this->programs[Noesis::Shader::SDF_Linear] = { 8, 11 };
    this->programs[Noesis::Shader::SDF_Radial] = { 8, 12 };
    this->programs[Noesis::Shader::SDF_Pattern] = { 8, 13 };

    this->programs[Noesis::Shader::SDF_LCD_Solid] = { 7, 14 };
    this->programs[Noesis::Shader::SDF_LCD_Linear] = { 8, 15 };
    this->programs[Noesis::Shader::SDF_LCD_Radial] = { 8, 16 };
    this->programs[Noesis::Shader::SDF_LCD_Pattern] = { 8, 17 };

    this->programs[Noesis::Shader::Image_Opacity_Solid] = { 5, 18 };
    this->programs[Noesis::Shader::Image_Opacity_Linear] = { 6, 19 };
    this->programs[Noesis::Shader::Image_Opacity_Radial] = { 6, 20 };
    this->programs[Noesis::Shader::Image_Opacity_Pattern] = { 6, 21 };

    this->programs[Noesis::Shader::Image_Shadow35V] = { 9, 22 };
    this->programs[Noesis::Shader::Image_Shadow63V] = { 9, 23 };
    this->programs[Noesis::Shader::Image_Shadow127V] = { 9, 24 };

    this->programs[Noesis::Shader::Image_Shadow35H_Solid] = { 9, 25 };
    this->programs[Noesis::Shader::Image_Shadow35H_Linear] = { 10, 26 };
    this->programs[Noesis::Shader::Image_Shadow35H_Radial] = { 10, 27 };
    this->programs[Noesis::Shader::Image_Shadow35H_Pattern] = { 10, 28 };

    this->programs[Noesis::Shader::Image_Shadow63H_Solid] = { 9, 29 };
    this->programs[Noesis::Shader::Image_Shadow63H_Linear] = { 10, 30 };
    this->programs[Noesis::Shader::Image_Shadow63H_Radial] = { 10, 31 };
    this->programs[Noesis::Shader::Image_Shadow63H_Pattern] = { 10, 32 };

    this->programs[Noesis::Shader::Image_Shadow127H_Solid] = { 9, 33 };
    this->programs[Noesis::Shader::Image_Shadow127H_Linear] = { 10, 34 };
    this->programs[Noesis::Shader::Image_Shadow127H_Radial] = { 10, 35 };
    this->programs[Noesis::Shader::Image_Shadow127H_Pattern] = { 10, 36 };

    this->programs[Noesis::Shader::Image_Blur35V] = { 9, 37 };
    this->programs[Noesis::Shader::Image_Blur63V] = { 9, 38 };
    this->programs[Noesis::Shader::Image_Blur127V] = { 9, 39 };

    this->programs[Noesis::Shader::Image_Blur35H_Solid] = { 9, 40 };
    this->programs[Noesis::Shader::Image_Blur35H_Linear] = { 10, 41 };
    this->programs[Noesis::Shader::Image_Blur35H_Radial] = { 10, 42 };
    this->programs[Noesis::Shader::Image_Blur35H_Pattern] = { 10, 43 };

    this->programs[Noesis::Shader::Image_Blur63H_Solid] = { 9, 44 };
    this->programs[Noesis::Shader::Image_Blur63H_Linear] = { 10, 45 };
    this->programs[Noesis::Shader::Image_Blur63H_Radial] = { 10, 46 };
    this->programs[Noesis::Shader::Image_Blur63H_Pattern] = { 10, 47 };

    this->programs[Noesis::Shader::Image_Blur127H_Solid] = { 9, 48 };
    this->programs[Noesis::Shader::Image_Blur127H_Linear] = { 10, 49 };
    this->programs[Noesis::Shader::Image_Blur127H_Radial] = { 10, 50 };
    this->programs[Noesis::Shader::Image_Blur127H_Pattern] = { 10, 51 };
}

void ff::internal::ui::render_device::clear_render_target()
{
    ff::graphics::dx11_device_state().set_layout_ia(nullptr);
    ff::graphics::dx11_device_state().set_gs(nullptr);
    ff::graphics::dx11_device_state().set_vs(this->quad_vs.shader());
    ff::graphics::dx11_device_state().set_ps(this->clear_ps.shader());
    ff::graphics::dx11_device_state().set_raster(this->rasterizer_states[2].Get());
    ff::graphics::dx11_device_state().set_blend(this->blend_states[0].Get(), ff::color::none(), 0xffffffff);
    ff::graphics::dx11_device_state().set_depth(this->depth_stencil_states[4].Get(), 0);
    ff::graphics::dx11_device_state().draw(3, 0);
}

void ff::internal::ui::render_device::clear_textures()
{
    ID3D11ShaderResourceView* textures[(size_t)texture_slot_t::Count] = { nullptr };
    ff::graphics::dx11_device_state().set_resources_ps(textures, 0, _countof(textures));
}

void ff::internal::ui::render_device::set_shaders(const Noesis::Batch& batch)
{
    const vertex_and_pixel_program_t& program = this->programs[batch.shader.v];
    vertex_shader_and_layout_t& vertex = this->vertex_stages[program.vertex_shader_index];
    pixel_shader_t& pixel = this->pixel_stages[program.pixel_shader_index];

    ff::graphics::dx11_device_state().set_layout_ia(vertex.layout());
    ff::graphics::dx11_device_state().set_vs(vertex.shader());
    ff::graphics::dx11_device_state().set_ps(pixel.shader());
}

void ff::internal::ui::render_device::set_buffers(const Noesis::Batch& batch)
{
    // Indices
    ff::graphics::dx11_device_state().set_index_ia(this->buffer_indices->buffer(), DXGI_FORMAT_R16_UINT, 0);

    // Vertices
    const vertex_and_pixel_program_t& program = this->programs[batch.shader.v];
    unsigned int stride = ::LAYOUT_FORMATS_AND_STRIDE[this->vertex_stages[program.vertex_shader_index].layout_index].second;
    ff::graphics::dx11_device_state().set_vertex_ia(this->buffer_vertices->buffer(), stride, batch.vertexOffset);

    // Vertex Constants
    if (this->vertex_cb_hash != batch.projMtxHash)
    {
        void* ptr = this->buffer_vertex_cb->map(16 * sizeof(float));
        ::memcpy(ptr, batch.projMtx, 16 * sizeof(float));
        this->buffer_vertex_cb->unmap();

        this->vertex_cb_hash = batch.projMtxHash;

    }

    // Pixel Constants
    if (batch.rgba != 0 || batch.radialGrad != 0 || batch.opacity != 0)
    {
        uint32_t hash = batch.rgbaHash ^ batch.radialGradHash ^ batch.opacityHash;
        if (this->pixel_cb_hash != hash)
        {
            void* ptr = this->buffer_pixel_cb->map(12 * sizeof(float));

            if (batch.rgba != 0)
            {
                memcpy(ptr, batch.rgba, 4 * sizeof(float));
                ptr = (uint8_t*)ptr + 4 * sizeof(float);
            }

            if (batch.radialGrad != 0)
            {
                memcpy(ptr, batch.radialGrad, 8 * sizeof(float));
                ptr = (uint8_t*)ptr + 8 * sizeof(float);
            }

            if (batch.opacity != 0)
            {
                memcpy(ptr, batch.opacity, sizeof(float));
            }

            this->buffer_pixel_cb->unmap();

            this->pixel_cb_hash = hash;
        }
    }

    // Texture dimensions
    if (batch.glyphs != 0 || batch.image != 0 || batch.pattern != 0)
    {
        ff::internal::ui::texture* image_texture = ff::internal::ui::texture::get(batch.glyphs ? batch.glyphs : batch.image);
        ff::internal::ui::texture* pattern_texture = ff::internal::ui::texture::get(batch.pattern);

        texture_dimensions_t data{};
        data.image_size = image_texture ? image_texture->internal_texture()->size().cast<float>() : ff::point_float(0, 0);
        data.image_inverse_size = image_texture ? ff::point_float(1, 1) / data.image_size : ff::point_float(0, 0);
        data.pattern_size = pattern_texture ? pattern_texture->internal_texture()->size().cast<float>() : ff::point_float(0, 0);
        data.pattern_inverse_size = pattern_texture ? ff::point_float(1, 1) / data.pattern_size : ff::point_float(0, 0);
        data.palette_row = static_cast<unsigned int>(ff::ui::global_palette() ? ff::ui::global_palette()->current_row() : 0);
        size_t hash = ff::stable_hash_func(data);

        if (this->texture_dimensions_cb_hash != hash)
        {
            texture_dimensions_t* mapped_data = reinterpret_cast<texture_dimensions_t*>(this->buffer_texture_dimensions_cb->map(TEX_DIMENSIONS_SIZE));
            std::memcpy(mapped_data, &data, TEX_DIMENSIONS_SIZE);
            this->buffer_texture_dimensions_cb->unmap();
            this->texture_dimensions_cb_hash = hash;
        }
    }

    // Effects
    if (batch.effectParamsSize != 0 && this->effect_cb_hash != batch.effectParamsHash)
    {
        void* ptr = this->buffer_effect_cb->map(16 * sizeof(float));
        std::memcpy(ptr, batch.effectParams, batch.effectParamsSize * sizeof(float));
        this->buffer_effect_cb->unmap();
        this->effect_cb_hash = batch.effectParamsHash;
    }
}

void ff::internal::ui::render_device::set_render_state(const Noesis::Batch& batch)
{
    Noesis::RenderState renderState = batch.renderState;

    uint32_t rasterizer_index = renderState.f.wireframe | (renderState.f.scissorEnable << 1);
    ID3D11RasterizerState* rasterizer = this->rasterizer_states[rasterizer_index].Get();
    ff::graphics::dx11_device_state().set_raster(rasterizer);

    uint32_t blend_index = renderState.f.colorEnable ? renderState.f.blendMode : 3;
    ID3D11BlendState* blend = this->blend_states[blend_index].Get();
    ff::graphics::dx11_device_state().set_blend(blend, ff::color::none(), 0xffffffff);

    uint32_t depth_index = renderState.f.stencilMode;
    ID3D11DepthStencilState* depth = this->depth_stencil_states[depth_index].Get();
    ff::graphics::dx11_device_state().set_depth(depth, batch.stencilRef);
}

void ff::internal::ui::render_device::set_textures(const Noesis::Batch& batch)
{
    if (batch.pattern)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.pattern);
        bool palette = ff::internal::palette_format(t->internal_texture()->format());
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
        ID3D11SamplerState* sampler = this->sampler_stages[batch.patternSampler.v].state();
        ff::graphics::dx11_device_state().set_resources_ps(views, (size_t)texture_slot_t::Pattern, 3);
        ff::graphics::dx11_device_state().set_samplers_ps(&sampler, (size_t)texture_slot_t::Pattern, 1);
    }

    if (batch.ramps)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.ramps);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_stages[batch.rampsSampler.v].state();
        ff::graphics::dx11_device_state().set_resources_ps(&view, (size_t)texture_slot_t::Ramps, 1);
        ff::graphics::dx11_device_state().set_samplers_ps(&sampler, (size_t)texture_slot_t::Ramps, 1);
    }

    if (batch.image)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.image);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_stages[batch.imageSampler.v].state();
        ff::graphics::dx11_device_state().set_resources_ps(&view, (size_t)texture_slot_t::Image, 1);
        ff::graphics::dx11_device_state().set_samplers_ps(&sampler, (size_t)texture_slot_t::Image, 1);
    }

    if (batch.glyphs)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.glyphs);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_stages[batch.glyphsSampler.v].state();
        ff::graphics::dx11_device_state().set_resources_ps(&view, (size_t)texture_slot_t::Glyphs, 1);
        ff::graphics::dx11_device_state().set_samplers_ps(&sampler, (size_t)texture_slot_t::Glyphs, 1);
    }

    if (batch.shadow)
    {
        ff::internal::ui::texture* t = ff::internal::ui::texture::get(batch.shadow);
        ID3D11ShaderResourceView* view = t->internal_texture()->view();
        ID3D11SamplerState* sampler = this->sampler_stages[batch.shadowSampler.v].state();
        ff::graphics::dx11_device_state().set_resources_ps(&view, (size_t)texture_slot_t::Shadow, 1);
        ff::graphics::dx11_device_state().set_samplers_ps(&sampler, (size_t)texture_slot_t::Shadow, 1);
    }
}

ID3D11VertexShader* ff::internal::ui::render_device::vertex_shader_t::shader()
{
    if (!this->shader_)
    {
        this->shader_ = ff::graphics::dx11_object_cache().get_vertex_shader(this->resource_name);
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
        this->shader_ = ff::graphics::dx11_object_cache().get_pixel_shader(this->resource_name);
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
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        desc.Filter = ::ToD3D(Noesis::MinMagFilter::Enum(this->params.f.minmagFilter), Noesis::MipFilter::Enum(this->params.f.mipFilter));
        ::ToD3D(Noesis::WrapMode::Enum(this->params.f.wrapMode), desc.AddressU, desc.AddressV);

        this->state_ = ff::graphics::dx11_object_cache().get_sampler_state(desc);
    }

    return this->state_.Get();
}
