#include "pch.h"
#include "dx12/access.h"
#include "dx12/buffer.h"
#include "dx12/depth.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/draw_device.h"
#include "dx12/dx12_globals.h"
#include "dx12/gpu_event.h"
#include "dx12/object_cache.h"
#include "dx12/texture.h"
#include "dx12/texture_view.h"
#include "dx12/queue.h"
#include "dx12/resource.h"
#include "dxgi/draw_util.h"
#include "dxgi/palette_data_base.h"
#include "dxgi/target_base.h"
#include "types/vertex.h"

#include "ff.dx12.res.id.h"

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

static void get_texture_sampler_desc(D3D12_FILTER filter, D3D12_SAMPLER_DESC& desc)
{
    desc = {};
    desc.Filter = filter;
    desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
}

static D3D12_VIEWPORT get_viewport(const ff::rect_float& view_rect)
{
    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = view_rect.left;
    viewport.TopLeftY = view_rect.top;
    viewport.Width = view_rect.width();
    viewport.Height = view_rect.height();
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;

    return viewport;
}

namespace
{
    class dx12_state
    {
    private:
        dx12_state(const D3D12_INPUT_ELEMENT_DESC* element_desc, size_t element_count)
            : input_layout_desc{ element_desc, static_cast<UINT>(element_count) }
        {}

        enum class state_t
        {
            blend_opaque = 0,
            blend_alpha = 0b0001,
            blend_pma = 0b0010,

            out_color = 0,
            out_palette = 0b0100,

            depth_disabled = 0,
            depth_enabled = 0b1000,

            target_default = 0,
            target_bgra = 0b10000,

            count = 0b100000
        };

    public:
        template<typename T>
        static ::dx12_state create()
        {
            return ::dx12_state(T::layout().data(), T::layout().size());
        }

        void reset(
            ID3D12RootSignature* root_signature,
            std::string_view vs_res,
            std::string_view gs_res,
            std::string_view ps_res,
            std::string_view ps_palette_out_res)
        {
            this->root_signature = root_signature;
            this->vs_res_name = vs_res;
            this->gs_res_name = gs_res;
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

        bool apply(
            ff::dx12::commands& commands,
            DXGI_FORMAT target_format,
            bool has_depth,
            bool alpha,
            bool pre_multiplied_alpha,
            bool palette_out)
        {
            assert(palette_out || target_format == DXGI_FORMAT_R8G8B8A8_UNORM || target_format == DXGI_FORMAT_B8G8R8A8_UNORM);
            assert(!palette_out || target_format == DXGI_FORMAT_R8_UINT);

            state_t index = ff::flags::combine(
                (alpha ? (pre_multiplied_alpha ? state_t::blend_pma : state_t::blend_alpha) : state_t::blend_opaque),
                (palette_out ? state_t::out_palette : state_t::out_color),
                (has_depth ? state_t::depth_enabled : state_t::depth_disabled),
                (target_format == DXGI_FORMAT_B8G8R8A8_UNORM ? state_t::target_bgra : state_t::target_default));

            auto& state = this->pipeline_states[static_cast<size_t>(index)];

            if (!state)
            {
                state = this->create_pipeline_state(index);
            }

            if (state)
            {
                commands.pipeline_state(state.Get());
                return true;
            }

            return false;
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineState> create_pipeline_state(state_t index) const
        {
            ff::resource_object_provider* shader_resources = &ff::internal::dx12::shader_resources();

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = this->root_signature.Get();
            desc.VS = ff::dx12::get_object_cache().shader(shader_resources, this->vs_res_name);
            desc.GS = ff::dx12::get_object_cache().shader(shader_resources, this->gs_res_name);
            desc.PS = ff::dx12::get_object_cache().shader(shader_resources, ff::flags::has(index, state_t::out_palette) ? this->ps_palette_out_res_name : this->ps_res_name);
            desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            desc.SampleMask = UINT_MAX;
            desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            desc.InputLayout = input_layout_desc;
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
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

            if (ff::flags::has(index, state_t::out_palette))
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
        std::string gs_res_name;
        std::string ps_res_name;
        std::string ps_palette_out_res_name;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        D3D12_INPUT_LAYOUT_DESC input_layout_desc;
        std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(state_t::count)> pipeline_states;
    };

    class dx12_draw_device : public ff::dxgi::draw_util::draw_device_base, public ff::dxgi::draw_device_base
    {
    public:
        dx12_draw_device()
            : samplers_gpu(ff::dx12::gpu_sampler_descriptors().alloc_pinned_range(2))
            , states_
        {
            ::dx12_state::create<ff::dx12::vertex::line_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::circle_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::triangle_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::sprite_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::sprite_geometry>(),

            ::dx12_state::create<ff::dx12::vertex::line_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::circle_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::triangle_geometry>(),
            ::dx12_state::create<ff::dx12::vertex::sprite_geometry>(),
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
                D3D12_SAMPLER_DESC point_desc, linear_desc;
                ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_POINT, point_desc);
                ::get_texture_sampler_desc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, linear_desc);

                ff::dx12::descriptor_range samplers_cpu = ff::dx12::cpu_sampler_descriptors().alloc_range(2);
                ff::dx12::device()->CreateSampler(&point_desc, samplers_cpu.cpu_handle(0));
                ff::dx12::device()->CreateSampler(&linear_desc, samplers_cpu.cpu_handle(1));
                ff::dx12::device()->CopyDescriptorsSimple(2, this->samplers_gpu.cpu_handle(0), samplers_cpu.cpu_handle(0), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }

            // Create root signature
            {
                CD3DX12_DESCRIPTOR_RANGE1 textures_range;
                textures_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 32, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 using_palette_textures;
                using_palette_textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 32, 32, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 palette_textures;
                palette_textures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 64, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0);

                CD3DX12_DESCRIPTOR_RANGE1 samplers_range;
                samplers_range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0);

                std::array<CD3DX12_ROOT_PARAMETER1, 7> params;
                params[0].InitAsConstants(static_cast<UINT>(ff::dxgi::draw_util::geometry_shader_constants_0::DWORD_COUNT), 0, 0, D3D12_SHADER_VISIBILITY_GEOMETRY); // geometry_constants_0
                params[1].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_GEOMETRY); // geometry_constants_1, matrixes
                params[2].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL); // pixel_constants_0, palette texture sizes
                params[3].InitAsDescriptorTable(1, &samplers_range, D3D12_SHADER_VISIBILITY_PIXEL); // samplers: point, linear
                params[4].InitAsDescriptorTable(1, &textures_range, D3D12_SHADER_VISIBILITY_PIXEL); // textures[32]
                params[5].InitAsDescriptorTable(1, &using_palette_textures, D3D12_SHADER_VISIBILITY_PIXEL); // palette textures[32]
                params[6].InitAsDescriptorTable(1, &palette_textures, D3D12_SHADER_VISIBILITY_PIXEL); // palette, remap

                D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
                D3D12_ROOT_SIGNATURE_DESC1& desc = versioned_desc.Desc_1_1;
                desc.Flags =
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
                    D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

                desc.NumParameters = static_cast<UINT>(params.size());
                desc.pParameters = params.data();

                this->root_signature = ff::dx12::get_object_cache().root_signature(versioned_desc);
            }

            // Pipeline states
            {
                ID3D12RootSignature* rs = this->root_signature.Get();

                this->state(ff::dxgi::draw_util::geometry_bucket_type::lines).reset(rs, assets::dx12::FF_DX12_LINE_VS, assets::dx12::FF_DX12_LINE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::circles).reset(rs, assets::dx12::FF_DX12_CIRCLE_VS, assets::dx12::FF_DX12_CIRCLE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::triangles).reset(rs, assets::dx12::FF_DX12_TRIANGLE_VS, assets::dx12::FF_DX12_TRIANGLE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::sprites).reset(rs, assets::dx12::FF_DX12_SPRITE_VS, assets::dx12::FF_DX12_SPRITE_GS, assets::dx12::FF_DX12_SPRITE_PS, assets::dx12::FF_DX12_PALETTE_OUT_SPRITE_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::palette_sprites).reset(rs, assets::dx12::FF_DX12_SPRITE_VS, assets::dx12::FF_DX12_SPRITE_GS, assets::dx12::FF_DX12_SPRITE_PALETTE_PS, assets::dx12::FF_DX12_PALETTE_OUT_SPRITE_PALETTE_PS);

                this->state(ff::dxgi::draw_util::geometry_bucket_type::lines_alpha).reset(rs, assets::dx12::FF_DX12_LINE_VS, assets::dx12::FF_DX12_LINE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::circles_alpha).reset(rs, assets::dx12::FF_DX12_CIRCLE_VS, assets::dx12::FF_DX12_CIRCLE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::triangles_alpha).reset(rs, assets::dx12::FF_DX12_TRIANGLE_VS, assets::dx12::FF_DX12_TRIANGLE_GS, assets::dx12::FF_DX12_COLOR_PS, assets::dx12::FF_DX12_PALETTE_OUT_COLOR_PS);
                this->state(ff::dxgi::draw_util::geometry_bucket_type::sprites_alpha).reset(rs, assets::dx12::FF_DX12_SPRITE_VS, assets::dx12::FF_DX12_SPRITE_GS, assets::dx12::FF_DX12_SPRITE_PS, assets::dx12::FF_DX12_PALETTE_OUT_SPRITE_PS);
            }
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

        virtual ff::dxgi::command_context_base* internal_setup(
            ff::dxgi::command_context_base& context,
            ff::dxgi::target_base& target,
            ff::dxgi::depth_base* depth,
            const ff::rect_float& view_rect,
            bool ignore_rotation) override
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);
            if (depth)
            {
                assert_ret_val(depth->physical_size(commands, target.size().physical_pixel_size()), nullptr);
                depth->clear(commands, 0, 0);
            }

            const ff::rect_float physical_view_rect = !ignore_rotation
                ? target.size().logical_to_physical_rect(view_rect)
                : view_rect;

            this->setup_target = &target;
            this->setup_viewport = ::get_viewport(physical_view_rect);
            this->setup_depth = depth;

            this->geometry_constants_version_0 = 0;
            this->geometry_constants_version_1 = 0;
            this->pixel_constants_version_0 = 0;

            this->commands = &commands;
            this->commands->begin_event(ff::dx12::gpu_event::draw_2d);
            this->commands->targets(&this->setup_target, 1, this->setup_depth);
            this->commands->viewports(&this->setup_viewport, 1);
            this->commands->scissors(nullptr, 1);
            this->commands->root_signature(this->root_signature.Get());
            this->commands->root_descriptors(3, this->samplers_gpu);

            return this->internal_flush(nullptr, false);
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
                        remap_image.pixels = const_cast<uint8_t*>(iter.second.first);
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

            if (this->geometry_constants_buffer_1_)
            {
                this->commands->resource_state(*this->geometry_constants_buffer_1_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            if (this->pixel_constants_buffer_0_)
            {
                this->commands->resource_state(*this->pixel_constants_buffer_0_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            for (size_t i = 0; i < texture_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures[i];
                this->commands->resource_state(
                    this->resource_from(view), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    view->view_array_start(), view->view_array_size(), view->view_mip_start(), view->view_mip_size());
            }

            for (size_t i = 0; i < textures_using_palette_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures_using_palette[i];
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

            if (this->geometry_constants_buffer_0_ && this->geometry_constants_version_0 != this->geometry_constants_buffer_0_.version())
            {
                this->geometry_constants_version_0 = this->geometry_constants_buffer_0_.version();
                this->commands->root_constants(0, this->geometry_constants_buffer_0_.data().data(), ff::dxgi::draw_util::geometry_shader_constants_0::DWORD_COUNT * 4);
            }

            if (geometry_constants_buffer_1_ && this->geometry_constants_version_1 != this->geometry_constants_buffer_1_.version())
            {
                this->geometry_constants_version_1 = this->geometry_constants_buffer_1_.version();
                this->commands->root_cbv(1, this->geometry_constants_buffer_1_);
            }

            if (this->pixel_constants_buffer_0_ && this->pixel_constants_version_0 != this->pixel_constants_buffer_0_.version())
            {
                this->pixel_constants_version_0 = this->pixel_constants_buffer_0_.version();
                this->commands->root_cbv(2, this->pixel_constants_buffer_0_);
            }

            // Update texture descriptors
            static std::vector<UINT> ones(std::max(ff::dxgi::draw_util::MAX_TEXTURES, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE), 1);

            if (texture_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(texture_count, this->commands->next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ff::dxgi::draw_util::MAX_TEXTURES> views;

                for (size_t i = 0; i < texture_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(texture_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                this->commands->root_descriptors(4, dest_range);
            }

            if (textures_using_palette_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(textures_using_palette_count, this->commands->next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE> views;

                for (size_t i = 0; i < textures_using_palette_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures_using_palette[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(textures_using_palette_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                this->commands->root_descriptors(5, dest_range);
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
                this->commands->root_descriptors(6, dest_range);
            }
        }

        virtual void apply_opaque_state(ff::dxgi::command_context_base& context) override
        {
            this->commands->primitive_topology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        }

        virtual void apply_alpha_state(ff::dxgi::command_context_base& context) override
        {
            this->commands->primitive_topology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        }

        virtual bool apply_geometry_state(ff::dxgi::command_context_base& context, const ff::dxgi::draw_util::geometry_bucket& bucket) override
        {
            if (this->state(bucket.bucket_type()).apply(*this->commands,
                this->setup_target->format(),
                this->setup_depth != nullptr,
                bucket.bucket_type() >= ff::dxgi::draw_util::geometry_bucket_type::first_alpha,
                this->pre_multiplied_alpha(),
                this->target_requires_palette()))
            {
                ff::dx12::buffer_base* single_buffer = &this->geometry_buffer_;
                D3D12_VERTEX_BUFFER_VIEW vertex_view = this->geometry_buffer_.vertex_view(bucket.item_size());
                this->commands->vertex_buffers(&single_buffer, &vertex_view, 0, 1);
                return true;
            }

            return false;
        }

        virtual ff::dxgi::buffer_base& geometry_buffer() override
        {
            return this->geometry_buffer_;
        }

        virtual ff::dxgi::buffer_base& geometry_constants_buffer_0() override
        {
            return this->geometry_constants_buffer_0_;
        }

        virtual ff::dxgi::buffer_base& geometry_constants_buffer_1() override
        {
            return this->geometry_constants_buffer_1_;
        }

        virtual ff::dxgi::buffer_base& pixel_constants_buffer_0() override
        {
            return this->pixel_constants_buffer_0_;
        }

        virtual bool flush_for_sampler_change() const override
        {
            return false;
        }

        virtual std::shared_ptr<ff::dxgi::texture_base> create_texture(ff::point_size size, DXGI_FORMAT format) override
        {
            return std::make_shared<ff::dx12::texture>(size, format);
        }

        virtual void draw(ff::dxgi::command_context_base& context, size_t count, size_t start) override
        {
            this->commands->draw(start, count);
        }

    private:
        ::dx12_state& state(ff::dxgi::draw_util::geometry_bucket_type type)
        {
            return this->states_[static_cast<size_t>(type)];
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
        ff::dx12::buffer_upload geometry_buffer_{ ff::dxgi::buffer_type::vertex };
        ff::dx12::buffer_cpu geometry_constants_buffer_0_{ ff::dxgi::buffer_type::constant }; // root constants
        ff::dx12::buffer geometry_constants_buffer_1_{ ff::dxgi::buffer_type::constant };
        ff::dx12::buffer pixel_constants_buffer_0_{ ff::dxgi::buffer_type::constant };
        size_t geometry_constants_version_0{};
        size_t geometry_constants_version_1{};
        size_t pixel_constants_version_0{};

        // Render state
        ff::dx12::descriptor_range samplers_gpu; // 0:point, 1:linear
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        std::array<::dx12_state, static_cast<size_t>(ff::dxgi::draw_util::geometry_bucket_type::count)> states_;

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
