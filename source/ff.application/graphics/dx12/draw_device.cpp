#include "pch.h"
#include "graphics/dx12/access.h"
#include "graphics/dx12/buffer.h"
#include "graphics/dx12/depth.h"
#include "graphics/dx12/descriptor_allocator.h"
#include "graphics/dx12/device_reset_priority.h"
#include "graphics/dx12/draw_device.h"
#include "graphics/dx12/dx12_globals.h"
#include "graphics/dx12/gpu_event.h"
#include "graphics/dx12/object_cache.h"
#include "graphics/dx12/texture.h"
#include "graphics/dx12/texture_view.h"
#include "graphics/dx12/queue.h"
#include "graphics/dx12/resource.h"
#include "graphics/dxgi/draw_util.h"
#include "graphics/dxgi/palette_data_base.h"
#include "graphics/dxgi/target_base.h"

#include "ff.dx12.res.id.h"

namespace ffdu = ff::dxgi::draw_util;

constexpr size_t ROOT_VS_CONSTANTS_0 = 0;
constexpr size_t ROOT_VS_CONSTANTS_1 = 1;
constexpr size_t ROOT_PS_CONSTANTS_0 = 2;
constexpr size_t ROOT_SAMPLERS = 3;
constexpr size_t ROOT_TEXTURES = 4;
constexpr size_t ROOT_PALETTE_TEXTURES = 5;
constexpr size_t ROOT_PALETTES = 6;
constexpr size_t VERTEX_VIEW_INDEX = 0;
constexpr size_t INSTANCE_VIEW_INDEX = 1;
constexpr size_t TRIANGLE_INDEX_START = 0;
constexpr size_t TRIANGLE_INDEX_COUNT = 3;
constexpr size_t RECTANGLE_INDEX_START = 0;
constexpr size_t RECTANGLE_INDEX_COUNT = 6;
constexpr size_t RECTANGLE_OUTLINE_INDEX_START = ::RECTANGLE_INDEX_START + ::RECTANGLE_INDEX_COUNT;
constexpr size_t RECTANGLE_OUTLINE_INDEX_COUNT = 24;
constexpr size_t CIRCLE_FILLED_INDEX_START = ::RECTANGLE_OUTLINE_INDEX_START + ::RECTANGLE_OUTLINE_INDEX_COUNT;
constexpr size_t CIRCLE_FILLED_INDEX_COUNT = 96; // 32 * 3
constexpr size_t CIRCLE_OUTLINE_INDEX_START = ::CIRCLE_FILLED_INDEX_START + ::CIRCLE_FILLED_INDEX_COUNT;
constexpr size_t CIRCLE_OUTLINE_INDEX_COUNT = 192; // 32 * 6

#define VERTEX_DESC(name, index, type) D3D12_INPUT_ELEMENT_DESC{ name, index, type, static_cast<UINT>(::VERTEX_VIEW_INDEX), D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
#define INSTANCE_DESC(name, index, type) D3D12_INPUT_ELEMENT_DESC{ name, index, type, static_cast<UINT>(::INSTANCE_VIEW_INDEX), D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }

static std::span<const D3D12_INPUT_ELEMENT_DESC> sprite_layout()
{
    static const std::array layout
    {
        INSTANCE_DESC("RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("POSROT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("INDEXES", 0, DXGI_FORMAT_R32_UINT),
    };

    return layout;
}

static std::span<const D3D12_INPUT_ELEMENT_DESC> line_layout()
{
    static const std::array layout
    {
        INSTANCE_DESC("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 1, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 2, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 3, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("THICKNESS", 1, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("INDEX", 0, DXGI_FORMAT_R32_UINT),
    };

    return layout;
}

static std::span<const D3D12_INPUT_ELEMENT_DESC> triangle_layout()
{
    static const std::array layout
    {
        INSTANCE_DESC("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 1, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 2, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("INDEX", 0, DXGI_FORMAT_R32_UINT),
    };

    return layout;
}

static std::span<const D3D12_INPUT_ELEMENT_DESC> rectangle_layout()
{
    static const std::array layout
    {
        INSTANCE_DESC("RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("DEPTH", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("INDEX", 0, DXGI_FORMAT_R32_UINT),
    };

    return layout;
}

static std::span<const D3D12_INPUT_ELEMENT_DESC> circle_layout()
{
    static const std::array layout
    {
        VERTEX_DESC("COSSIN", 0, DXGI_FORMAT_R32G32_FLOAT),
        INSTANCE_DESC("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT),
        INSTANCE_DESC("THICKNESS", 0, DXGI_FORMAT_R32_FLOAT),
        INSTANCE_DESC("INDEX", 0, DXGI_FORMAT_R32_UINT),
    };

    return layout;
}

static std::shared_ptr<ff::data_base> get_static_index_data()
{
    static const uint16_t indexes[] =
    {
        // RECTANGLE_INDEX_START|COUNT
        0, 1, 2, 2, 1, 3,

        // RECTANGLE_OUTLINE_INDEX_START|COUNT
        0, 1, 4, 4, 1, 5,
        1, 3, 5, 5, 3, 7,
        3, 2, 7, 7, 2, 6,
        2, 0, 6, 6, 0, 4,

        // CIRCLE_FILLED_INDEX_START|COUNT
        0, 64, 1,
        1, 64, 2,
        2, 64, 3,
        3, 64, 4,
        4, 64, 5,
        5, 64, 6,
        6, 64, 7,
        7, 64, 8,
        8, 64, 9,
        9, 64, 10,
        10, 64, 11,
        11, 64, 12,
        12, 64, 13,
        13, 64, 14,
        14, 64, 15,
        15, 64, 16,
        16, 64, 17,
        17, 64, 18,
        18, 64, 19,
        19, 64, 20,
        20, 64, 21,
        21, 64, 22,
        22, 64, 23,
        23, 64, 24,
        24, 64, 25,
        25, 64, 26,
        26, 64, 27,
        27, 64, 28,
        28, 64, 29,
        29, 64, 30,
        30, 64, 31,
        31, 64, 0,

        // CIRCLE_OUTLINE_INDEX_START|COUNT
        0, 32, 1, 1, 32, 33,
        1, 33, 2, 2, 33, 34,
        2, 34, 3, 3, 34, 35,
        3, 35, 4, 4, 35, 36,
        4, 36, 5, 5, 36, 37,
        5, 37, 6, 6, 37, 38,
        6, 38, 7, 7, 38, 39,
        7, 39, 8, 8, 39, 40,
        8, 40, 9, 9, 40, 41,
        9, 41, 10, 10, 41, 42,
        10, 42, 11, 11, 42, 43,
        11, 43, 12, 12, 43, 44,
        12, 44, 13, 13, 44, 45,
        13, 45, 14, 14, 45, 46,
        14, 46, 15, 15, 46, 47,
        15, 47, 16, 16, 47, 48,
        16, 48, 17, 17, 48, 49,
        17, 49, 18, 18, 49, 50,
        18, 50, 19, 19, 50, 51,
        19, 51, 20, 20, 51, 52,
        20, 52, 21, 21, 52, 53,
        21, 53, 22, 22, 53, 54,
        22, 54, 23, 23, 54, 55,
        23, 55, 24, 24, 55, 56,
        24, 56, 25, 25, 56, 57,
        25, 57, 26, 26, 57, 58,
        26, 58, 27, 27, 58, 59,
        27, 59, 28, 28, 59, 60,
        28, 60, 29, 29, 60, 61,
        29, 61, 30, 30, 61, 62,
        30, 62, 31, 31, 62, 63,
        31, 63, 0, 0, 63, 32,
    };

    static_assert(_countof(indexes) ==
        ::RECTANGLE_INDEX_COUNT +
        ::RECTANGLE_OUTLINE_INDEX_COUNT +
        ::CIRCLE_FILLED_INDEX_COUNT +
        ::CIRCLE_OUTLINE_INDEX_COUNT);

    return std::make_shared<ff::data_static>(&indexes[0], _countof(indexes) * sizeof(indexes[0]));
}

static std::shared_ptr<ff::data_base> get_static_vertex_data()
{
    static const DirectX::XMFLOAT2 vertexes[] =
    {
        // Outside of cirlce, indexes 0 - 31
        { 1.000000f, 0.000000f }, // 0 - 90 degrees, indexes 0 - 7
        { 0.980785f, 0.195090f },
        { 0.923880f, 0.382683f },
        { 0.831470f, 0.555570f },
        { 0.707107f, 0.707107f },
        { 0.555570f, 0.831470f },
        { 0.382683f, 0.923880f },
        { 0.195090f, 0.980785f },
        { 0.000000f, 1.000000f }, // 90 - 180 degrees, indexes 8 - 15
        { -0.195090f, 0.980785f },
        { -0.382683f, 0.923880f },
        { -0.555570f, 0.831470f },
        { -0.707107f, 0.707107f },
        { -0.831470f, 0.555570f },
        { -0.923880f, 0.382683f },
        { -0.980785f, 0.195090f },
        { -1.000000f, 0.000000f }, // 180 - 270 degrees, indexes 16 - 23
        { -0.980785f, -0.195090f },
        { -0.923880f, -0.382683f },
        { -0.831470f, -0.555570f },
        { -0.707107f, -0.707107f },
        { -0.555570f, -0.831470f },
        { -0.382683f, -0.923880f },
        { -0.195090f, -0.980785f },
        { -0.000000f, -1.000000f }, // 270 - 360 degrees, indexes 24 - 31
        { 0.195090f, -0.980785f },
        { 0.382683f, -0.923880f },
        { 0.555570f, -0.831470f },
        { 0.707107f, -0.707107f },
        { 0.831470f, -0.555570f },
        { 0.923880f, -0.382683f },
        { 0.980785f, -0.195090f }, // end of 360 degrees, index 31

        // Inside of cirlce, indexes 32 - 63
        { 1.000000f, 0.000000f }, // 0 - 90 degrees, indexes 32 - 39
        { 0.980785f, 0.195090f },
        { 0.923880f, 0.382683f },
        { 0.831470f, 0.555570f },
        { 0.707107f, 0.707107f },
        { 0.555570f, 0.831470f },
        { 0.382683f, 0.923880f },
        { 0.195090f, 0.980785f },
        { 0.000000f, 1.000000f }, // 90 - 180 degrees, indexes 40 - 47
        { -0.195090f, 0.980785f },
        { -0.382683f, 0.923880f },
        { -0.555570f, 0.831470f },
        { -0.707107f, 0.707107f },
        { -0.831470f, 0.555570f },
        { -0.923880f, 0.382683f },
        { -0.980785f, 0.195090f },
        { -1.000000f, 0.000000f }, // 180 - 270 degrees, indexes 48 - 55
        { -0.980785f, -0.195090f },
        { -0.923880f, -0.382683f },
        { -0.831470f, -0.555570f },
        { -0.707107f, -0.707107f },
        { -0.555570f, -0.831470f },
        { -0.382683f, -0.923880f },
        { -0.195090f, -0.980785f },
        { -0.000000f, -1.000000f }, // 270 - 360 degrees, indexes 56 - 63
        { 0.195090f, -0.980785f },
        { 0.382683f, -0.923880f },
        { 0.555570f, -0.831470f },
        { 0.707107f, -0.707107f },
        { 0.831470f, -0.555570f },
        { 0.923880f, -0.382683f },
        { 0.980785f, -0.195090f }, // end of 360 degrees, index 63

        // Center of circle, index 64
        { 0.000000f, 0.000000f },
    };

    return std::make_shared<ff::data_static>(&vertexes[0], _countof(vertexes) * sizeof(vertexes[0]));
}

static bool target_format_valid(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R8_UINT:
            return true;
    }

    return false;
}

static void get_alpha_blend_desc(D3D12_RENDER_TARGET_BLEND_DESC& desc)
{
    // newColor = (srcColor * SrcBlend) BlendOp (destColor * DestBlend)
    // newAlpha = (srcAlpha * SrcBlendAlpha) BlendOpAlpha (destAlpha * DestBlendAlpha)

    desc.BlendEnable = TRUE;
    desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendOp = D3D12_BLEND_OP_ADD;
    desc.SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
}

static void get_pre_multiplied_alpha_blend_desc(D3D12_RENDER_TARGET_BLEND_DESC& desc)
{
    // newColor = (srcColor * SrcBlend) BlendOp (destColor * DestBlend)
    // newAlpha = (srcAlpha * SrcBlendAlpha) BlendOpAlpha (destAlpha * DestBlendAlpha)

    desc.BlendEnable = TRUE;
    desc.SrcBlend = D3D12_BLEND_ONE;
    desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendOp = D3D12_BLEND_OP_ADD;
    desc.SrcBlendAlpha = D3D12_BLEND_ONE;
    desc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
}

static D3D12_SAMPLER_DESC get_texture_sampler_desc(D3D12_FILTER filter)
{
    return D3D12_SAMPLER_DESC
    {
        .Filter = filter,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .MaxLOD = D3D12_FLOAT32_MAX,
    };
}

static D3D12_VIEWPORT get_viewport(const ff::rect_float& view_rect)
{
    return D3D12_VIEWPORT
    {
        .TopLeftX = view_rect.left,
        .TopLeftY = view_rect.top,
        .Width = view_rect.width(),
        .Height = view_rect.height(),
        .MinDepth = 0,
        .MaxDepth = 1,
    };
}

namespace
{
    class dx12_state
    {
    private:
        enum class state_t
        {
            blend_opaque = 0b00,
            blend_alpha = 0b01,
            blend_pma = 0b10,

            depth_disabled = 0b000,
            depth_enabled = 0b100,

            target_default = 0b00000,
            target_bgra = 0b01000,
            target_palette = 0b10000,

            count = 0b100000
        };

    public:
        dx12_state(std::span<const D3D12_INPUT_ELEMENT_DESC> elements)
            : input_layout_desc{ elements.data(), static_cast<UINT>(elements.size()) }
        {
        }

        void reset(ID3D12RootSignature* root_signature, std::string_view vs_res, std::string_view ps_res, std::string_view ps_palette_out_res)
        {
            this->root_signature = root_signature;
            this->vs_res_name = vs_res;
            this->ps_res_name = ps_res;
            this->ps_palette_out_res_name = ps_palette_out_res;

            this->reset();
        }

        void reset()
        {
            for (auto& i : this->pipeline_states)
            {
                i.Reset();
            }
        }

        bool apply(ff::dx12::commands& commands, DXGI_FORMAT target_format, bool has_depth, bool transparent, bool pre_multiplied_alpha)
        {
            state_t index = ff::flags::combine(
                has_depth ? state_t::depth_enabled : state_t::depth_disabled,
                (transparent && pre_multiplied_alpha && target_format != DXGI_FORMAT_R8_UINT) ? state_t::blend_pma : state_t::blend_opaque,
                (transparent && !pre_multiplied_alpha && target_format != DXGI_FORMAT_R8_UINT) ? state_t::blend_alpha : state_t::blend_opaque,
                (target_format == DXGI_FORMAT_B8G8R8A8_UNORM) ? state_t::target_bgra : state_t::target_default,
                (target_format == DXGI_FORMAT_R8_UINT) ? state_t::target_palette : state_t::target_default);

            auto& state = this->pipeline_states[static_cast<size_t>(index)];
            if (!state && !(state = this->create_pipeline_state(index)))
            {
                return false;
            }

            commands.pipeline_state(state.Get());
            return true;
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> create_pipeline_state(state_t index) const
        {
            ff::resource_object_provider* shader_resources = &ff::internal::dx12::shader_resources();

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = this->root_signature.Get();
            desc.VS = ff::dx12::get_object_cache().shader(shader_resources, this->vs_res_name);
            desc.PS = ff::dx12::get_object_cache().shader(shader_resources, ff::flags::has(index, state_t::target_palette) ? this->ps_palette_out_res_name : this->ps_res_name);
            desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            desc.SampleMask = UINT_MAX;
            desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            desc.InputLayout = input_layout_desc;
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.SampleDesc.Count = 1;

            if (ff::flags::has(index, state_t::blend_alpha))
            {
                ::get_alpha_blend_desc(desc.BlendState.RenderTarget[0]);
            }
            else if (ff::flags::has(index, state_t::blend_pma))
            {
                ::get_pre_multiplied_alpha_blend_desc(desc.BlendState.RenderTarget[0]);
            }

            if (ff::flags::has(index, state_t::depth_enabled))
            {
                desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
                desc.DSVFormat = ff::dx12::depth::FORMAT;
            }
            else
            {
                desc.DepthStencilState.DepthEnable = FALSE;
                desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            }

            if (ff::flags::has(index, state_t::target_palette))
            {
                desc.RTVFormats[0] = DXGI_FORMAT_R8_UINT;
            }
            else if (ff::flags::has(index, state_t::target_bgra))
            {
                desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
            }
            else
            {
                desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            }

            return ff::dx12::get_object_cache().pipeline_state(desc);
        }

    private:
        std::string vs_res_name;
        std::string ps_res_name;
        std::string ps_palette_out_res_name;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        D3D12_INPUT_LAYOUT_DESC input_layout_desc;
        std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(state_t::count)> pipeline_states;
    };

    class dx12_draw_device : public ffdu::draw_device_base, public ff::dxgi::draw_device_base
    {
    public:
        dx12_draw_device()
            : samplers_gpu(ff::dx12::gpu_sampler_descriptors().alloc_pinned_range(2))
            , states_
            {
                sprite_layout(),
                sprite_layout(),
                line_layout(),
                triangle_layout(),
                rectangle_layout(),
                rectangle_layout(),
                circle_layout(),
                circle_layout(),
            }
        {
            this->as_device_child()->reset();
            ff::dx12::add_device_child(this->as_device_child(), ff::dx12::device_reset_priority::normal);
        }

        virtual ~dx12_draw_device() override
        {
            ff::dx12::remove_device_child(this->as_device_child());
        }

        dx12_draw_device(dx12_draw_device&& other) noexcept = delete;
        dx12_draw_device(const dx12_draw_device& other) = delete;
        dx12_draw_device& operator=(dx12_draw_device&& other) noexcept = delete;
        dx12_draw_device& operator=(const dx12_draw_device& other) = delete;

        virtual bool valid() const override
        {
            return this->internal_valid();
        }

        virtual ff::dxgi::draw_ptr begin_draw(
            ff::dxgi::command_context_base& context,
            ff::dxgi::target_base& target,
            ff::dxgi::depth_base* depth,
            const ff::rect_float& view_rect,
            const ff::rect_float& world_rect,
            ff::dxgi::draw_options options) override
        {
            return this->internal_begin_draw(context, target, depth, view_rect, world_rect, options);
        }

    protected:
        virtual void internal_destroy() override
        {
            assert(!this->commands);

            for (auto& bucket : this->states_)
            {
                bucket.reset();
            }
        }

        virtual void internal_reset() override
        {
            // Create samplers
            {
                D3D12_SAMPLER_DESC point_desc = ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_POINT);
                D3D12_SAMPLER_DESC linear_desc = ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_LINEAR);

                ff::dx12::descriptor_range samplers_cpu = ff::dx12::cpu_sampler_descriptors().alloc_range(2);
                ff::dx12::device()->CreateSampler(&point_desc, samplers_cpu.cpu_handle(0));
                ff::dx12::device()->CreateSampler(&linear_desc, samplers_cpu.cpu_handle(1));
                ff::dx12::device()->CopyDescriptorsSimple(2, this->samplers_gpu.cpu_handle(0), samplers_cpu.cpu_handle(0), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }

            // Create root signature
            {
                CD3DX12_DESCRIPTOR_RANGE1 samplers_range;
                samplers_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 textures_range;
                textures_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(ffdu::MAX_TEXTURES), 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 palette_textures_range;
                palette_textures_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(ffdu::MAX_PALETTE_TEXTURES), static_cast<UINT>(ffdu::MAX_TEXTURES), 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 palette_textures;
                palette_textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, static_cast<UINT>(ffdu::MAX_TEXTURES + ffdu::MAX_PALETTE_TEXTURES), 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, 0);

                std::array<CD3DX12_ROOT_PARAMETER1, 7> params;
                params[::ROOT_VS_CONSTANTS_0].InitAsConstants(static_cast<UINT>(ffdu::vs_constants_0::DWORD_COUNT), 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
                params[::ROOT_VS_CONSTANTS_1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
                params[::ROOT_PS_CONSTANTS_0].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
                params[::ROOT_SAMPLERS].InitAsDescriptorTable(1, &samplers_range, D3D12_SHADER_VISIBILITY_PIXEL);
                params[::ROOT_TEXTURES].InitAsDescriptorTable(1, &textures_range, D3D12_SHADER_VISIBILITY_PIXEL);
                params[::ROOT_PALETTE_TEXTURES].InitAsDescriptorTable(1, &palette_textures_range, D3D12_SHADER_VISIBILITY_PIXEL);
                params[::ROOT_PALETTES].InitAsDescriptorTable(1, &palette_textures, D3D12_SHADER_VISIBILITY_PIXEL);

                D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
                D3D12_ROOT_SIGNATURE_DESC1& desc = versioned_desc.Desc_1_1;
                desc.Flags =
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

                desc.NumParameters = static_cast<UINT>(params.size());
                desc.pParameters = params.data();

                this->root_signature = ff::dx12::get_object_cache().root_signature(versioned_desc);
                this->root_signature->SetName(L"dx12_draw_device root signature");
            }

            // Pipeline states
            {
                ID3D12RootSignature* rs = this->root_signature.Get();
                namespace a = assets::dx12;

                this->state(ffdu::instance_bucket_type::sprites).reset(rs, a::FF_DX12_VS_SPRITE, a::FF_DX12_PS_SPRITE, a::FF_DX12_PS_SPRITE_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::palette_sprites).reset(rs, a::FF_DX12_VS_SPRITE, a::FF_DX12_PS_PALETTE_SPRITE, a::FF_DX12_PS_PALETTE_SPRITE_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::lines).reset(rs, a::FF_DX12_VS_LINE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::triangles).reset(rs, a::FF_DX12_VS_TRIANGLE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::rectangles_filled).reset(rs, a::FF_DX12_VS_RECTANGLE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::rectangles_outline).reset(rs, a::FF_DX12_VS_RECTANGLE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::circles_filled).reset(rs, a::FF_DX12_VS_CIRCLE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
                this->state(ffdu::instance_bucket_type::circles_outline).reset(rs, a::FF_DX12_VS_CIRCLE, a::FF_DX12_PS_COLOR, a::FF_DX12_PS_COLOR_OUT_PALETTE);
            }
        }

        virtual ff::dxgi::command_context_base* internal_setup(
            ff::dxgi::command_context_base& context,
            ff::dxgi::target_base& target,
            ff::dxgi::depth_base* depth,
            const ff::rect_float& view_rect,
            bool ignore_rotation) override
        {
            assert_msg_ret_val(::target_format_valid(target.format()), "Invalid target format", nullptr);

            ff::dx12::commands& commands = ff::dx12::commands::get(context);
            if (depth)
            {
                assert_ret_val(depth->physical_size(commands, target.size().physical_pixel_size()), nullptr);
                depth->clear(commands, 0, 0);
            }

            this->setup_target = &target;
            this->setup_viewport = ::get_viewport(!ignore_rotation ? target.size().logical_to_physical_rect(view_rect) : view_rect);
            this->setup_depth = depth;

            this->vs_constants_version_0 = 0;
            this->vs_constants_version_1 = 0;
            this->ps_constants_version_0 = 0;

            this->commands = &commands;
            this->commands->begin_event(ff::dx12::gpu_event::draw_2d);
            this->commands->targets(&this->setup_target, 1, this->setup_depth);
            this->commands->viewports(&this->setup_viewport, 1);
            this->commands->scissors(nullptr, 1);
            this->commands->root_signature(this->root_signature.Get());
            this->commands->root_descriptors(::ROOT_SAMPLERS, this->samplers_gpu);
            this->commands->primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            return this->commands;
        }

        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base* context, bool end_draw) override
        {
            if (end_draw)
            {
                this->commands->end_event();
                this->commands = nullptr;
            }

            return this->commands;
        }

        virtual void internal_flush_begin(ff::dxgi::command_context_base* context) override
        {
            this->commands->begin_event(ff::dx12::gpu_event::draw_batch);
        }

        virtual void internal_flush_end(ff::dxgi::command_context_base* context) override
        {
            this->commands->end_event();
        }

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) override
        {
            this->commands->begin_event(ff::dx12::gpu_event::update_palette);

            if (textures_using_palette_count && !palette_to_index.empty())
            {
                ff::dx12::texture& dest_texture = ff::dx12::texture::get(palette_texture);

                for (const auto& iter : palette_to_index)
                {
                    ff::dxgi::palette_base* palette = iter.second.first;
                    if (palette)
                    {
                        unsigned int index = iter.second.second;
                        size_t palette_row = palette->current_row();
                        const ff::dxgi::palette_data_base* palette_data = palette->data();
                        size_t row_hash = palette_data->row_hash(palette_row);

                        if (palette_texture_hashes[index] != row_hash)
                        {
                            palette_texture_hashes[index] = row_hash;
                            ff::dx12::texture& src_texture = ff::dx12::texture::get(*palette_data->texture());
                            this->commands->copy_texture(
                                *dest_texture.dx12_resource_updated(*this->commands), 0, ff::point_size(0, index),
                                *src_texture.dx12_resource_updated(*this->commands), 0, ff::rect_size(0, palette_row, ff::dxgi::palette_size, palette_row + 1));
                        }
                    }
                }
            }

            if ((textures_using_palette_count || this->target_requires_palette()) && !palette_remap_to_index.empty())
            {
                ff::dx12::texture& dest_remap_texture = ff::dx12::texture::get(palette_remap_texture);
                DXGI_FORMAT dest_format = dest_remap_texture.dx12_resource_updated(*this->commands)->desc().Format;
                DirectX::Image remap_image{ ff::dxgi::palette_size, 1, dest_format, ff::dxgi::palette_size, ff::dxgi::palette_size };

                for (const auto& iter : palette_remap_to_index)
                {
                    unsigned int row = iter.second.second;
                    size_t row_hash = iter.first;

                    if (palette_remap_texture_hashes[row] != row_hash)
                    {
                        palette_remap_texture_hashes[row] = row_hash;
                        const ff::dxgi::remap_t& row_remap = iter.second.first;
                        remap_image.pixels = const_cast<uint8_t*>(row_remap.remap.data());
                        dest_remap_texture.update(*this->commands, 0, 0, ff::point_size(0, row), remap_image);
                    }
                }
            }

            this->commands->end_event();
        }

        virtual void apply_shader_input(ff::dxgi::command_context_base& context,
            size_t texture_count, ff::dxgi::texture_view_base** in_textures,
            size_t textures_using_palette_count, ff::dxgi::texture_view_base** in_textures_using_palette,
            ff::dxgi::texture_base& palette_texture, ff::dxgi::texture_base& palette_remap_texture) override
        {
            // Prepare all resource state ahead of time

            if (this->vs_constants_buffer_1_)
            {
                this->commands->resource_state(*this->vs_constants_buffer_1_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            if (this->ps_constants_buffer_0_)
            {
                this->commands->resource_state(*this->ps_constants_buffer_0_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            // Vertex and index buffers
            {
                ff::dx12::buffer_base* single_buffer = &this->vertex_buffer;
                D3D12_VERTEX_BUFFER_VIEW vertex_view = this->vertex_buffer.vertex_view(sizeof(DirectX::XMFLOAT2));
                this->commands->vertex_buffers(&single_buffer, &vertex_view, ::VERTEX_VIEW_INDEX, 1);

                D3D12_INDEX_BUFFER_VIEW index_view = this->index_buffer.index_view();
                this->commands->index_buffer(this->index_buffer, index_view);
            }

            for (size_t i = 0; i < texture_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures[i];
                assert(&ff::dx12::target_access::get(*this->setup_target).dx12_target_texture() != &this->resource_from(view));
                this->commands->resource_state(
                    this->resource_from(view), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    view->view_array_start(), view->view_array_size(), view->view_mip_start(), view->view_mip_size());
            }

            for (size_t i = 0; i < textures_using_palette_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures_using_palette[i];
                assert(&ff::dx12::target_access::get(*this->setup_target).dx12_target_texture() != &this->resource_from(view));
                this->commands->resource_state(
                    this->resource_from(view), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    view->view_array_start(), view->view_array_size(), view->view_mip_start(), view->view_mip_size());
            }

            if (textures_using_palette_count || this->target_requires_palette())
            {
                this->commands->resource_state(this->resource_from(palette_texture), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                this->commands->resource_state(this->resource_from(palette_remap_texture), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }

            // Update root constants

            if (this->vs_constants_buffer_0_ && this->vs_constants_version_0 != this->vs_constants_buffer_0_.version())
            {
                this->vs_constants_version_0 = this->vs_constants_buffer_0_.version();
                this->commands->root_constants(::ROOT_VS_CONSTANTS_0, this->vs_constants_buffer_0_.data(), ffdu::vs_constants_0::DWORD_COUNT * 4);
            }

            if (vs_constants_buffer_1_ && this->vs_constants_version_1 != this->vs_constants_buffer_1_.version())
            {
                this->vs_constants_version_1 = this->vs_constants_buffer_1_.version();
                this->commands->root_cbv(::ROOT_VS_CONSTANTS_1, this->vs_constants_buffer_1_);
            }

            if (this->ps_constants_buffer_0_ && this->ps_constants_version_0 != this->ps_constants_buffer_0_.version())
            {
                this->ps_constants_version_0 = this->ps_constants_buffer_0_.version();
                this->commands->root_cbv(::ROOT_PS_CONSTANTS_0, this->ps_constants_buffer_0_);
            }

            // Update texture descriptors
            static std::vector<UINT> ones(std::max(ffdu::MAX_TEXTURES, ffdu::MAX_PALETTE_TEXTURES), 1);

            if (texture_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(texture_count, this->commands->next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ffdu::MAX_TEXTURES> views;

                for (size_t i = 0; i < texture_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(texture_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                this->commands->root_descriptors(::ROOT_TEXTURES, dest_range);
            }

            if (textures_using_palette_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(textures_using_palette_count, this->commands->next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ffdu::MAX_PALETTE_TEXTURES> views;

                for (size_t i = 0; i < textures_using_palette_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures_using_palette[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(textures_using_palette_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                this->commands->root_descriptors(::ROOT_PALETTE_TEXTURES, dest_range);
            }

            if (textures_using_palette_count || this->target_requires_palette())
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(2, this->commands->next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> views
                {
                    ff::dx12::texture_view_access::get(palette_texture).dx12_texture_view(),
                    ff::dx12::texture_view_access::get(palette_remap_texture).dx12_texture_view(),
                };

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(views.size());
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                this->commands->root_descriptors(::ROOT_PALETTES, dest_range);
            }
        }

        virtual bool apply_instance_state(ff::dxgi::command_context_base& context, const ffdu::instance_bucket& bucket) override
        {
            if (this->state(bucket.bucket_type()).apply(*this->commands,
                this->setup_target->format(),
                this->setup_depth != nullptr,
                bucket.is_transparent(),
                this->pre_multiplied_alpha()))
            {
                ff::dx12::buffer_base* single_buffer = &this->instance_buffer_;
                D3D12_VERTEX_BUFFER_VIEW instance_view = this->instance_buffer_.vertex_view(bucket.item_size());
                this->commands->vertex_buffers(&single_buffer, &instance_view, ::INSTANCE_VIEW_INDEX, 1);

                return true;
            }

            return false;
        }

        virtual ff::dxgi::buffer_base& instance_buffer() override
        {
            return this->instance_buffer_;
        }

        virtual ff::dxgi::buffer_base& vs_constants_buffer_0() override
        {
            return this->vs_constants_buffer_0_;
        }

        virtual ff::dxgi::buffer_base& vs_constants_buffer_1() override
        {
            return this->vs_constants_buffer_1_;
        }

        virtual ff::dxgi::buffer_base& ps_constants_buffer_0() override
        {
            return this->ps_constants_buffer_0_;
        }

        virtual void draw(ff::dxgi::command_context_base& context, ffdu::instance_bucket_type instance_type, size_t instance_start, size_t instance_count) override
        {
            switch (instance_type)
            {
                default:
                    debug_fail_msg("Invalid instance type");
                    break;

                case ffdu::instance_bucket_type::sprites:
                case ffdu::instance_bucket_type::sprites_out_transparent:
                case ffdu::instance_bucket_type::palette_sprites:
                case ffdu::instance_bucket_type::palette_sprites_out_transparent:
                case ffdu::instance_bucket_type::rectangles_filled:
                case ffdu::instance_bucket_type::rectangles_filled_out_transparent:
                case ffdu::instance_bucket_type::lines:
                case ffdu::instance_bucket_type::lines_out_transparent:
                    this->commands->draw_indexed(0, ::RECTANGLE_INDEX_START, ::RECTANGLE_INDEX_COUNT, instance_start, instance_count);
                    break;

                case ffdu::instance_bucket_type::rectangles_outline:
                case ffdu::instance_bucket_type::rectangles_outline_out_transparent:
                    this->commands->draw_indexed(0, ::RECTANGLE_OUTLINE_INDEX_START, ::RECTANGLE_OUTLINE_INDEX_COUNT, instance_start, instance_count);
                    break;

                case ffdu::instance_bucket_type::triangles:
                case ffdu::instance_bucket_type::triangles_out_transparent:
                    this->commands->draw_indexed(0, ::TRIANGLE_INDEX_START, ::TRIANGLE_INDEX_COUNT, instance_start, instance_count);
                    break;

                case ffdu::instance_bucket_type::circles_filled:
                case ffdu::instance_bucket_type::circles_filled_out_transparent:
                    this->commands->draw_indexed(0, ::CIRCLE_FILLED_INDEX_START, ::CIRCLE_FILLED_INDEX_COUNT, instance_start, instance_count);
                    break;

                case ffdu::instance_bucket_type::circles_outline:
                case ffdu::instance_bucket_type::circles_outline_out_transparent:
                    this->commands->draw_indexed(0, ::CIRCLE_OUTLINE_INDEX_START, ::CIRCLE_OUTLINE_INDEX_COUNT, instance_start, instance_count);
                    break;
            }
        }

    private:
        ::dx12_state& state(ffdu::instance_bucket_type type)
        {
            size_t index = static_cast<size_t>(type);
            if (type >= ffdu::instance_bucket_type::first_transparent)
            {
                index -= static_cast<size_t>(ffdu::instance_bucket_type::first_transparent);
            }

            return this->states_[index];
        }

        ff::dx12::resource& resource_from(ff::dxgi::texture_base& texture_base)
        {
            ff::dx12::texture& texture = ff::dx12::texture::get(texture_base);
            return *texture.dx12_resource_updated(*this->commands);
        }

        ff::dx12::resource& resource_from(ff::dxgi::texture_view_base* view)
        {
            return this->resource_from(*view->view_texture());
        }

        // Constant data for shaders
        ff::dx12::buffer_gpu instance_buffer_{ ff::dxgi::buffer_type::vertex };
        ff::dx12::buffer_gpu_static index_buffer{ ff::dxgi::buffer_type::index, ::get_static_index_data() };
        ff::dx12::buffer_gpu_static vertex_buffer{ ff::dxgi::buffer_type::vertex, ::get_static_vertex_data() };
        ff::dx12::buffer_cpu vs_constants_buffer_0_{ ff::dxgi::buffer_type::constant }; // root constants
        ff::dx12::buffer_gpu vs_constants_buffer_1_{ ff::dxgi::buffer_type::constant };
        ff::dx12::buffer_gpu ps_constants_buffer_0_{ ff::dxgi::buffer_type::constant };
        size_t vs_constants_version_0{};
        size_t vs_constants_version_1{};
        size_t ps_constants_version_0{};

        // Render state
        ff::dx12::descriptor_range samplers_gpu; // 0:point, 1:linear
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        std::array<::dx12_state, static_cast<size_t>(ffdu::instance_bucket_type::count) / 2> states_;

        ff::dx12::commands* commands{};
        ff::dxgi::target_base* setup_target{};
        ff::dxgi::depth_base* setup_depth{};
        D3D12_VIEWPORT setup_viewport{};
    };
}

std::unique_ptr<ff::dxgi::draw_device_base> ff::dx12::create_draw_device()
{
    return std::make_unique<::dx12_draw_device>();
}
