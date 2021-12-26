#include "pch.h"
#include "buffer.h"
#include "buffer_cpu.h"
#include "buffer_upload.h"
#include "depth.h"
#include "descriptor_allocator.h"
#include "device_reset_priority.h"
#include "draw_device.h"
#include "globals.h"
#include "object_cache.h"
#include "target_access.h"
#include "texture.h"
#include "texture_view.h"
#include "queue.h"
#include "resource.h"
#include "vertex.h"

static void get_alpha_blend(D3D12_RENDER_TARGET_BLEND_DESC& desc)
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
            ff::dxgi::command_context_base& context,
            DXGI_FORMAT target_format,
            bool has_depth,
            bool alpha,
            bool pre_multiplied_alpha,
            bool palette_out)
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);
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
                ff::dx12::commands::get(context).pipeline_state(state.Get());
                return true;
            }

            return false;
        }

        Microsoft::WRL::ComPtr<ID3D12PipelineStateX> create_pipeline_state(state_t index) const
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = this->root_signature.Get();
            desc.VS = ff::dx12::get_object_cache().shader(this->vs_res_name);
            desc.GS = ff::dx12::get_object_cache().shader(this->gs_res_name);
            desc.PS = ff::dx12::get_object_cache().shader(ff::flags::has(index, state_t::out_palette) ? this->ps_palette_out_res_name : this->ps_res_name);
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
                ::get_alpha_blend(desc.BlendState.RenderTarget[0]);
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
        std::array<Microsoft::WRL::ComPtr<ID3D12PipelineStateX>, static_cast<size_t>(state_t::count)> pipeline_states;
    };

    class dx12_draw_device : public ff::dxgi::draw_util::draw_device_base, public ff::dx12::draw_device
    {
    public:
        dx12_draw_device()
            : geometry_buffer_(ff::dxgi::buffer_type::vertex)
            , geometry_constants_buffer_0_(ff::dxgi::buffer_type::constant)
            , geometry_constants_buffer_1_(ff::dxgi::buffer_type::constant)
            , pixel_constants_buffer_0_(ff::dxgi::buffer_type::constant)
            , geometry_constants_version_0(0)
            , geometry_constants_version_1(0)
            , pixel_constants_version_0(0)
            , samplers_gpu(ff::dx12::gpu_sampler_descriptors().alloc_pinned_range(2))
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

        virtual ff::dxgi::draw_ptr begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::dxgi::draw_options options) override
        {
            return this->internal_begin_draw(target, depth, view_rect, world_rect, options);
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

                this->state(ff::dxgi::draw_util::geometry_bucket_type::lines).reset(rs, "ff.dx12.line_vs", "ff.dx12.line_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::circles).reset(rs, "ff.dx12.circle_vs", "ff.dx12.circle_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::triangles).reset(rs, "ff.dx12.triangle_vs", "ff.dx12.triangle_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::sprites).reset(rs, "ff.dx12.sprite_vs", "ff.dx12.sprite_gs", "ff.dx12.sprite_ps", "ff.dx12.palette_out_sprite_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::palette_sprites).reset(rs, "ff.dx12.sprite_vs", "ff.dx12.sprite_gs", "ff.dx12.sprite_palette_ps", "ff.dx12.palette_out_sprite_palette_ps");

                this->state(ff::dxgi::draw_util::geometry_bucket_type::lines_alpha).reset(rs, "ff.dx12.line_vs", "ff.dx12.line_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::circles_alpha).reset(rs, "ff.dx12.circle_vs", "ff.dx12.circle_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::triangles_alpha).reset(rs, "ff.dx12.triangle_vs", "ff.dx12.triangle_gs", "ff.dx12.color_ps", "ff.dx12.palette_out_color_ps");
                this->state(ff::dxgi::draw_util::geometry_bucket_type::sprites_alpha).reset(rs, "ff.dx12.sprite_vs", "ff.dx12.sprite_gs", "ff.dx12.sprite_ps", "ff.dx12.palette_out_sprite_ps");
            }
        }

        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base* context, bool end_draw) override
        {
            this->commands.reset();

            if (!end_draw)
            {
                this->geometry_constants_version_0 = 0;
                this->geometry_constants_version_1 = 0;
                this->pixel_constants_version_0 = 0;

                this->commands = std::make_unique<ff::dx12::commands>(ff::dx12::direct_queue().new_commands());
                this->commands->targets(&this->setup_target, 1, this->setup_depth);
                this->commands->viewports(&this->setup_viewport, 1);
                this->commands->root_signature(this->root_signature.Get());
                this->commands->root_descriptors(3, this->samplers_gpu.gpu_handle(0));
            }

            return this->commands.get();
        }

        virtual ff::dxgi::command_context_base* internal_setup(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect) override
        {
            this->setup_target = &target;
            this->setup_viewport = ::get_viewport(ff::dxgi::draw_util::get_rotated_view_rect(target, view_rect));
            this->setup_depth = depth;

            return this->internal_flush(nullptr, false);
        }

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) override
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);

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
                            commands.copy_texture(
                                *dest_texture.resource(), 0, ff::point_size(0, index),
                                *src_texture.resource(), 0, ff::rect_size(0, palette_row, ff::dxgi::palette_size, palette_row + 1));
                        }
                    }
                }
            }

            if ((textures_using_palette_count || this->target_requires_palette()) && !palette_remap_to_index.empty())
            {
                ff::dx12::texture& dest_remap_texture = ff::dx12::texture::get(palette_remap_texture);
                DXGI_FORMAT dest_format = dest_remap_texture.resource()->desc().Format;
                DirectX::Image remap_image{ ff::dxgi::palette_size, 1, dest_format, ff::dxgi::palette_size, ff::dxgi::palette_size };

                for (const auto& iter : palette_remap_to_index)
                {
                    unsigned int row = iter.second.second;
                    size_t row_hash = iter.first;

                    if (palette_remap_texture_hashes[row] != row_hash)
                    {
                        palette_remap_texture_hashes[row] = row_hash;
                        remap_image.pixels = const_cast<uint8_t*>(iter.second.first);
                        dest_remap_texture.resource()->update_texture(&commands, &remap_image, 0, 1, ff::point_size(0, row));
                    }
                }
            }
        }

        virtual void apply_shader_input(ff::dxgi::command_context_base& context,
            size_t texture_count, ff::dxgi::texture_view_base** in_textures,
            size_t textures_using_palette_count, ff::dxgi::texture_view_base** in_textures_using_palette,
            ff::dxgi::texture_base& palette_texture, ff::dxgi::texture_base& palette_remap_texture) override
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);

            // Prepare all resource state ahead of time

            if (this->geometry_constants_buffer_1_)
            {
                commands.resource_state(*this->geometry_constants_buffer_1_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            if (this->pixel_constants_buffer_0_)
            {
                commands.resource_state(*this->pixel_constants_buffer_0_.resource(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            for (size_t i = 0; i < texture_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures[i];
                commands.resource_state(
                    *ff::dx12::texture::get(*view->view_texture()).resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    view->view_array_start(), view->view_array_size(), view->view_mip_start(), view->view_mip_size());
            }

            for (size_t i = 0; i < textures_using_palette_count; i++)
            {
                ff::dxgi::texture_view_base* view = in_textures_using_palette[i];
                commands.resource_state(
                    *ff::dx12::texture::get(*view->view_texture()).resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                    view->view_array_start(), view->view_array_size(), view->view_mip_start(), view->view_mip_size());
            }

            if (textures_using_palette_count || this->target_requires_palette())
            {
                commands.resource_state(*ff::dx12::texture::get(palette_texture).resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                commands.resource_state(*ff::dx12::texture::get(palette_remap_texture).resource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }

            // Update root constants

            if (this->geometry_constants_buffer_0_ && this->geometry_constants_version_0 != this->geometry_constants_buffer_0_.version())
            {
                this->geometry_constants_version_0 = this->geometry_constants_buffer_0_.version();
                commands.root_constants(0, this->geometry_constants_buffer_0_.data().data(), ff::dxgi::draw_util::geometry_shader_constants_0::DWORD_COUNT * 4);
            }

            if (geometry_constants_buffer_1_ && this->geometry_constants_version_1 != this->geometry_constants_buffer_1_.version())
            {
                this->geometry_constants_version_1 = this->geometry_constants_buffer_1_.version();
                commands.root_cbv(1, this->geometry_constants_buffer_1_);
            }

            if (this->pixel_constants_buffer_0_ && this->pixel_constants_version_0 != this->pixel_constants_buffer_0_.version())
            {
                this->pixel_constants_version_0 = this->pixel_constants_buffer_0_.version();
                commands.root_cbv(2, this->pixel_constants_buffer_0_);
            }

            // Update texture descriptors
            static std::vector<UINT> ones(std::max(ff::dxgi::draw_util::MAX_TEXTURES, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE), 1);

            if (texture_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(texture_count, commands.next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ff::dxgi::draw_util::MAX_TEXTURES> views;

                for (size_t i = 0; i < texture_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(texture_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                commands.root_descriptors(4, dest_range.gpu_handle(0));
            }

            if (textures_using_palette_count)
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(textures_using_palette_count, commands.next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE> views;

                for (size_t i = 0; i < textures_using_palette_count; i++)
                {
                    views[i] = ff::dx12::texture_view_access::get(*in_textures_using_palette[i]).dx12_texture_view();
                }

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(textures_using_palette_count);
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                commands.root_descriptors(5, dest_range.gpu_handle(0));
            }

            if (textures_using_palette_count || this->target_requires_palette())
            {
                ff::dx12::descriptor_range dest_range = ff::dx12::gpu_view_descriptors().alloc_range(2, commands.next_fence_value());
                std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 2> views
                {
                    ff::dx12::texture_view_access::get(palette_texture).dx12_texture_view(),
                    ff::dx12::texture_view_access::get(palette_remap_texture).dx12_texture_view(),
                };

                D3D12_CPU_DESCRIPTOR_HANDLE dest = dest_range.cpu_handle(0);
                UINT size = static_cast<UINT>(views.size());
                ff::dx12::device()->CopyDescriptors(1, &dest, &size, size, views.data(), ones.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                commands.root_descriptors(6, dest_range.gpu_handle(0));
            }
        }

        virtual void apply_opaque_state(ff::dxgi::command_context_base& context) override
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);
            commands.primitive_topology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        }

        virtual void apply_alpha_state(ff::dxgi::command_context_base& context) override
        {
            ff::dx12::commands& commands = ff::dx12::commands::get(context);
            commands.primitive_topology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        }

        virtual bool apply_geometry_state(ff::dxgi::command_context_base& context, const ff::dxgi::draw_util::geometry_bucket& bucket) override
        {
            if (this->state(bucket.bucket_type()).apply(context,
                this->setup_target->format(),
                this->setup_depth != nullptr,
                bucket.bucket_type() >= ff::dxgi::draw_util::geometry_bucket_type::first_alpha,
                this->pre_multiplied_alpha(),
                this->target_requires_palette()))
            {
                ff::dx12::commands& commands = ff::dx12::commands::get(context);
                //ff::dx12::resource* single_resource = this->geometry_buffer_.resource();
                //commands.vertex_buffers(&single_resource, &this->geometry_buffer_.vertex_view(bucket.item_size()), 0, 1);
                commands.vertex_buffers(nullptr, &this->geometry_buffer_.vertex_view(bucket.item_size()), 0, 1);

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
            ff::dx12::commands::get(context).draw(start, count);
        }

    private:
        ::dx12_state& state(ff::dxgi::draw_util::geometry_bucket_type type)
        {
            return this->states_[static_cast<size_t>(type)];
        }

        // Constant data for shaders
        ff::dx12::buffer_upload geometry_buffer_;
        ff::dx12::buffer_cpu geometry_constants_buffer_0_; // root constants
        ff::dx12::buffer geometry_constants_buffer_1_;
        ff::dx12::buffer pixel_constants_buffer_0_;
        size_t geometry_constants_version_0, geometry_constants_version_1, pixel_constants_version_0;

        // Render state
        ff::dx12::descriptor_range samplers_gpu; // 0:point, 1:linear
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        ff::dxgi::target_base* setup_target{};
        ff::dxgi::depth_base* setup_depth{};
        D3D12_VIEWPORT setup_viewport{};

        // Render data
        std::array<::dx12_state, static_cast<size_t>(ff::dxgi::draw_util::geometry_bucket_type::count)> states_;
        std::unique_ptr<ff::dx12::commands> commands;
    };
}

std::unique_ptr<ff::dx12::draw_device> ff::dx12::draw_device::create()
{
    return std::make_unique<::dx12_draw_device>();
}

ff::dxgi::draw_ptr ff::dx12::draw_device::begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::dxgi::draw_options options)
{
    return this->begin_draw(target, depth, std::floor(view_rect).cast<float>(), std::floor(world_rect).cast<float>(), options);
}
