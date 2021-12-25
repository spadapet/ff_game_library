#include "pch.h"
#include "buffer.h"
#include "depth.h"
#include "device_reset_priority.h"
#include "device_state.h"
#include "draw_device.h"
#include "fixed_state.h"
#include "globals.h"
#include "object_cache.h"
#include "target_access.h"
#include "texture.h"
#include "texture_view.h"
#include "vertex.h"

static std::array<ID3D11ShaderResourceView*, ff::dxgi::draw_util::MAX_TEXTURES + ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE + 2 /* palette + remap */>  NULL_TEXTURES{};

namespace
{
    class dx11_shaders
    {
    private:
        dx11_shaders(size_t item_size, const D3D11_INPUT_ELEMENT_DESC* element_desc, size_t element_count)
            : item_size(item_size)
            , element_desc(element_desc)
            , element_count(element_count)
        {}

    public:
        template<typename T>
        static ::dx11_shaders create()
        {
            return ::dx11_shaders(sizeof(T), T::layout().data(), T::layout().size());
        }

        void reset(std::string_view vs_res, std::string_view gs_res, std::string_view ps_res, std::string_view ps_palette_out_res)
        {
            this->vs_res_name = vs_res;
            this->gs_res_name = gs_res;
            this->ps_res_name = ps_res;
            this->ps_palette_out_res_name = ps_palette_out_res;

            this->reset();
        }

        void reset()
        {
            this->layout.Reset();
            this->vs.Reset();
            this->gs.Reset();
            this->ps.Reset();
            this->ps_palette_out.Reset();
        }

        void apply(ff::dxgi::command_context_base& context, ff::dxgi::buffer_base& geometry_buffer, bool palette_out)
        {
            if (!this->vs)
            {
                this->vs = ff::dx11::get_object_cache().get_vertex_shader_and_input_layout(this->vs_res_name, this->layout, this->element_desc, this->element_count);
            }

            if (!this->gs)
            {
                this->gs = ff::dx11::get_object_cache().get_geometry_shader(this->gs_res_name);
            }

            Microsoft::WRL::ComPtr<ID3D11PixelShader>& ps = palette_out ? this->ps_palette_out : this->ps;
            if (!ps)
            {
                ps = ff::dx11::get_object_cache().get_pixel_shader(palette_out ? this->ps_palette_out_res_name : this->ps_res_name);
            }

            ff::dx11::device_state& state = ff::dx11::device_state::get(context);
            state.set_vertex_ia(ff::dx11::buffer::get(geometry_buffer).dx_buffer(), this->item_size, 0);
            state.set_layout_ia(this->layout.Get());
            state.set_vs(this->vs.Get());
            state.set_gs(this->gs.Get());
            state.set_ps(palette_out ? this->ps_palette_out.Get() : this->ps.Get());
        }

    private:
        std::string vs_res_name;
        std::string gs_res_name;
        std::string ps_res_name;
        std::string ps_palette_out_res_name;

        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_palette_out;

        const D3D11_INPUT_ELEMENT_DESC* element_desc;
        size_t element_count;
        size_t item_size;
    };
}

static void get_alpha_blend(D3D11_RENDER_TARGET_BLEND_DESC& desc)
{
    // newColor = (srcColor * SrcBlend) BlendOp (destColor * DestBlend)
    // newAlpha = (srcAlpha * SrcBlendAlpha) BlendOpAlpha (destAlpha * DestBlendAlpha)

    desc.BlendEnable = TRUE;
    desc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.BlendOp = D3D11_BLEND_OP_ADD;
    desc.SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
}

static void get_pre_multiplied_alpha_blend(D3D11_RENDER_TARGET_BLEND_DESC& desc)
{
    // newColor = (srcColor * SrcBlend) BlendOp (destColor * DestBlend)
    // newAlpha = (srcAlpha * SrcBlendAlpha) BlendOpAlpha (destAlpha * DestBlendAlpha)

    desc.BlendEnable = TRUE;
    desc.SrcBlend = D3D11_BLEND_ONE;
    desc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.BlendOp = D3D11_BLEND_OP_ADD;
    desc.SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
}

static ID3D11SamplerState* get_texture_sampler_state(D3D11_FILTER filter)
{
    CD3D11_SAMPLER_DESC sampler(D3D11_DEFAULT);
    sampler.Filter = filter;
    return ff::dx11::get_object_cache().get_sampler_state(sampler);
}

static ID3D11BlendState* get_opaque_blend_state()
{
    CD3D11_BLEND_DESC blend(D3D11_DEFAULT);
    return ff::dx11::get_object_cache().get_blend_state(blend);
}

static ID3D11BlendState* get_alpha_blend_state()
{
    CD3D11_BLEND_DESC blend(D3D11_DEFAULT);
    ::get_alpha_blend(blend.RenderTarget[0]);
    return ff::dx11::get_object_cache().get_blend_state(blend);
}

static ID3D11BlendState* get_pre_multiplied_alpha_blend_state()
{
    CD3D11_BLEND_DESC blend(D3D11_DEFAULT);
    ::get_pre_multiplied_alpha_blend(blend.RenderTarget[0]);
    return ff::dx11::get_object_cache().get_blend_state(blend);
}

static ID3D11DepthStencilState* get_enabled_depth_state()
{
    CD3D11_DEPTH_STENCIL_DESC depth(D3D11_DEFAULT);
    depth.DepthFunc = D3D11_COMPARISON_GREATER;
    return ff::dx11::get_object_cache().get_depth_stencil_state(depth);
}

static ID3D11DepthStencilState* get_disabled_depth_state()
{
    CD3D11_DEPTH_STENCIL_DESC depth(D3D11_DEFAULT);
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    return ff::dx11::get_object_cache().get_depth_stencil_state(depth);
}

static ID3D11RasterizerState* get_no_cull_raster_state()
{
    CD3D11_RASTERIZER_DESC raster(D3D11_DEFAULT);
    raster.CullMode = D3D11_CULL_NONE;
    return ff::dx11::get_object_cache().get_rasterize_state(raster);
}

static ff::dx11::fixed_state create_opaque_draw_state()
{
    ff::dx11::fixed_state state;

    state.blend = ::get_opaque_blend_state();
    state.depth = ::get_enabled_depth_state();
    state.disabled_depth = ::get_disabled_depth_state();
    state.raster = ::get_no_cull_raster_state();

    return state;
}

static ff::dx11::fixed_state create_alpha_draw_state()
{
    ff::dx11::fixed_state state;

    state.blend = ::get_alpha_blend_state();
    state.depth = ::get_enabled_depth_state();
    state.disabled_depth = ::get_disabled_depth_state();
    state.raster = ::get_no_cull_raster_state();

    return state;
}

static ff::dx11::fixed_state create_pre_multiplied_alpha_draw_state()
{
    ff::dx11::fixed_state state;

    state.blend = ::get_pre_multiplied_alpha_blend_state();
    state.depth = ::get_enabled_depth_state();
    state.disabled_depth = ::get_disabled_depth_state();
    state.raster = ::get_no_cull_raster_state();

    return state;
}

static D3D11_VIEWPORT get_viewport(const ff::rect_float& view_rect)
{
    D3D11_VIEWPORT viewport;
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
    class dx11_draw_device : public ff::dxgi::draw_util::draw_device_base, public ff::dx11::draw_device
    {
    public:
        dx11_draw_device()
            : geometry_buffer_(ff::dxgi::buffer_type::vertex)
            , geometry_constants_buffer_0_(ff::dxgi::buffer_type::constant)
            , geometry_constants_buffer_1_(ff::dxgi::buffer_type::constant)
            , pixel_constants_buffer_0_(ff::dxgi::buffer_type::constant)
            , geometry_bucket_shaders
        {
            ::dx11_shaders::create<ff::dx11::vertex::line_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::circle_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::triangle_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::sprite_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::sprite_geometry>(),

            ::dx11_shaders::create<ff::dx11::vertex::line_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::circle_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::triangle_geometry>(),
            ::dx11_shaders::create<ff::dx11::vertex::sprite_geometry>(),
        }
        {
            this->as_device_child()->reset();
            ff::dx11::add_device_child(this->as_device_child(), ff::dx11::device_reset_priority::normal);
        }

        virtual ~dx11_draw_device() override
        {
            ff::dx11::remove_device_child(this->as_device_child());
        }

        dx11_draw_device(dx11_draw_device&& other) noexcept = delete;
        dx11_draw_device(const dx11_draw_device& other) = delete;
        dx11_draw_device& operator=(dx11_draw_device&& other) noexcept = delete;
        dx11_draw_device& operator=(const dx11_draw_device& other) = delete;

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
            this->sampler_state_linear.Reset();
            this->sampler_state_point.Reset();
            this->opaque_state = ff::dx11::fixed_state();
            this->alpha_state = ff::dx11::fixed_state();
            this->pre_multiplied_alpha_state = ff::dx11::fixed_state();

            for (auto& bucket : this->geometry_bucket_shaders)
            {
                bucket.reset();
            }
        }

        virtual void internal_reset() override
        {
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::lines).reset("ff.dx11.line_vs", "ff.dx11.line_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::circles).reset("ff.dx11.circle_vs", "ff.dx11.circle_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::triangles).reset("ff.dx11.triangle_vs", "ff.dx11.triangle_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::sprites).reset("ff.dx11.sprite_vs", "ff.dx11.sprite_gs", "ff.dx11.sprite_ps", "ff.dx11.palette_out_sprite_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::palette_sprites).reset("ff.dx11.sprite_vs", "ff.dx11.sprite_gs", "ff.dx11.sprite_palette_ps", "ff.dx11.palette_out_sprite_palette_ps");

            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::lines_alpha).reset("ff.dx11.line_vs", "ff.dx11.line_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::circles_alpha).reset("ff.dx11.circle_vs", "ff.dx11.circle_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::triangles_alpha).reset("ff.dx11.triangle_vs", "ff.dx11.triangle_gs", "ff.dx11.color_ps", "ff.dx11.palette_out_color_ps");
            this->get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type::sprites_alpha).reset("ff.dx11.sprite_vs", "ff.dx11.sprite_gs", "ff.dx11.sprite_ps", "ff.dx11.palette_out_sprite_ps");

            // States
            this->sampler_state_linear = ::get_texture_sampler_state(D3D11_FILTER_MIN_MAG_MIP_LINEAR);
            this->sampler_state_point = ::get_texture_sampler_state(D3D11_FILTER_MIN_MAG_MIP_POINT);
            this->opaque_state = ::create_opaque_draw_state();
            this->alpha_state = ::create_alpha_draw_state();
            this->pre_multiplied_alpha_state = ::create_pre_multiplied_alpha_draw_state();
        }

        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base* context, bool end_draw) override
        {
            if (end_draw)
            {
                ff::dx11::device_state::get(*context).set_resources_ps(::NULL_TEXTURES.data(), 0, ::NULL_TEXTURES.size());
            }

            return context;
        }

        virtual ff::dxgi::command_context_base* internal_setup(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect) override
        {
            ID3D11RenderTargetView* target_view = ff::dx11::target_access::get(target).dx11_target_view();
            ID3D11DepthStencilView* depth_view = depth ? ff::dx11::depth::get(*depth).view() : nullptr;

            ff::rect_float rotated_view_rect = ff::dxgi::draw_util::get_rotated_view_rect(target, view_rect);
            D3D11_VIEWPORT viewport = ::get_viewport(rotated_view_rect);

            ff::dx11::device_state& state = ff::dx11::get_device_state();
            state.set_targets(&target_view, 1, depth_view);
            state.set_viewports(&viewport, 1);

            return &state;
        }

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) override
        {
            ff::dx11::device_state& state = ff::dx11::device_state::get(context);

            if (textures_using_palette_count && !palette_to_index.empty())
            {
                ID3D11Resource* dest_resource = ff::dx11::texture::get(palette_texture).dx11_texture();
                CD3D11_BOX box(0, 0, 0, static_cast<int>(ff::dxgi::palette_size), 1, 1);

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
                            ID3D11Resource* src_resource = ff::dx11::texture::get(*palette_data->texture()).dx11_texture();
                            box.top = static_cast<UINT>(palette_row);
                            box.bottom = box.top + 1;
                            state.copy_subresource_region(dest_resource, 0, 0, index, 0, src_resource, 0, &box);
                        }
                    }
                }
            }

            if ((textures_using_palette_count || this->target_requires_palette()) && !palette_remap_to_index.empty())
            {
                ID3D11Resource* dest_remap_resource = ff::dx11::texture::get(palette_remap_texture).dx11_texture();
                CD3D11_BOX box(0, 0, 0, static_cast<int>(ff::dxgi::palette_size), 1, 1);

                for (const auto& iter : palette_remap_to_index)
                {
                    const uint8_t* remap = iter.second.first;
                    unsigned int row = iter.second.second;
                    size_t row_hash = iter.first;

                    if (palette_remap_texture_hashes[row] != row_hash)
                    {
                        palette_remap_texture_hashes[row] = row_hash;
                        box.top = row;
                        box.bottom = row + 1;
                        state.update_subresource(dest_remap_resource, 0, &box, remap, static_cast<UINT>(ff::dxgi::palette_size), 0);
                    }
                }
            }
        }

        virtual void apply_shader_input(ff::dxgi::command_context_base& context,
            size_t texture_count, ff::dxgi::texture_view_base** in_textures,
            size_t textures_using_palette_count, ff::dxgi::texture_view_base** in_textures_using_palette,
            ff::dxgi::texture_base& palette_texture, ff::dxgi::texture_base& palette_remap_texture) override
        {
            ff::dx11::device_state& state = ff::dx11::device_state::get(context);

            std::array<ID3D11Buffer*, 2> buffers_gs = { this->geometry_constants_buffer_0_.dx_buffer(), this->geometry_constants_buffer_1_.dx_buffer() };
            state.set_constants_gs(buffers_gs.data(), 0, buffers_gs.size());

            std::array<ID3D11Buffer*, 1> buffers_ps = { this->pixel_constants_buffer_0_.dx_buffer() };
            state.set_constants_ps(buffers_ps.data(), 0, buffers_ps.size());

            std::array<ID3D11SamplerState*, 1> sample_states = { this->linear_sampler() ? this->sampler_state_linear.Get() : this->sampler_state_point.Get() };
            state.set_samplers_ps(sample_states.data(), 0, sample_states.size());

            if (texture_count)
            {
                std::array<ID3D11ShaderResourceView*, ff::dxgi::draw_util::MAX_TEXTURES> textures;
                for (size_t i = 0; i < texture_count; i++)
                {
                    textures[i] = ff::dx11::texture_view::get(*in_textures[i]).dx11_texture_view();
                }

                state.set_resources_ps(textures.data(), 0, texture_count);
            }

            if (textures_using_palette_count)
            {
                std::array<ID3D11ShaderResourceView*, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE> textures_using_palette;
                for (size_t i = 0; i < textures_using_palette_count; i++)
                {
                    textures_using_palette[i] = ff::dx11::texture_view::get(*in_textures_using_palette[i]).dx11_texture_view();
                }

                state.set_resources_ps(textures_using_palette.data(), ff::dxgi::draw_util::MAX_TEXTURES, textures_using_palette_count);
            }

            if (textures_using_palette_count || this->target_requires_palette())
            {
                std::array<ID3D11ShaderResourceView*, 2> palettes =
                {
                    (textures_using_palette_count ? ff::dx11::texture_view::get(palette_texture).dx11_texture_view() : nullptr),
                    ff::dx11::texture_view::get(palette_remap_texture).dx11_texture_view(),
                };

                state.set_resources_ps(palettes.data(), ff::dxgi::draw_util::MAX_TEXTURES + ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE, palettes.size());
            }
        }

        virtual void apply_opaque_state(ff::dxgi::command_context_base& context) override
        {
            ff::dx11::device_state::get(context).set_topology_ia(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
            this->opaque_state.apply();
        }

        virtual void apply_alpha_state(ff::dxgi::command_context_base& context) override
        {
            ff::dx11::device_state::get(context).set_topology_ia(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
            ff::dx11::fixed_state& alpha_state = this->pre_multiplied_alpha() ? this->pre_multiplied_alpha_state : this->alpha_state;
            alpha_state.apply();
        }

        virtual bool apply_geometry_state(ff::dxgi::command_context_base& context, const ff::dxgi::draw_util::geometry_bucket& bucket) override
        {
            this->get_geometry_bucket_shaders(bucket.bucket_type()).apply(context, this->geometry_buffer_, this->target_requires_palette());
            return true;
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
            return true;
        }

        virtual std::shared_ptr<ff::dxgi::texture_base> create_texture(ff::point_size size, DXGI_FORMAT format) override
        {
            return std::make_shared<ff::dx11::texture>(size, format);
        }

        virtual void draw(ff::dxgi::command_context_base& context, size_t count, size_t start) override
        {
            ff::dx11::device_state::get(context).draw(count, start);
        }

    private:
        ::dx11_shaders& get_geometry_bucket_shaders(ff::dxgi::draw_util::geometry_bucket_type type)
        {
            return this->geometry_bucket_shaders[static_cast<size_t>(type)];
        }

        // Constant data for shaders
        ff::dx11::buffer geometry_buffer_;
        ff::dx11::buffer geometry_constants_buffer_0_;
        ff::dx11::buffer geometry_constants_buffer_1_;
        ff::dx11::buffer pixel_constants_buffer_0_;

        // Render state
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state_linear;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_state_point;
        ff::dx11::fixed_state opaque_state;
        ff::dx11::fixed_state alpha_state;
        ff::dx11::fixed_state pre_multiplied_alpha_state;

        // Render data
        std::array<::dx11_shaders, static_cast<size_t>(ff::dxgi::draw_util::geometry_bucket_type::count)> geometry_bucket_shaders;
    };
}

std::unique_ptr<ff::dx11::draw_device> ff::dx11::draw_device::create()
{
    return std::make_unique<::dx11_draw_device>();
}

ff::dxgi::draw_ptr ff::dx11::draw_device::begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::dxgi::draw_options options)
{
    return this->begin_draw(target, depth, std::floor(view_rect).cast<float>(), std::floor(world_rect).cast<float>(), options);
}
