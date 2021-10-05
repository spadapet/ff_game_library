#include "pch.h"
#include "buffer.h"
#include "depth.h"
#include "draw_device.h"
#include "graphics.h"
#include "matrix.h"
#include "matrix_stack.h"
#include "palette_data.h"
#include "shader.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "target_base.h"
#include "texture.h"
#include "texture_view_base.h"
#include "transform.h"
#include "vertex.h"

#if DXVER == 11

static const size_t MAX_TEXTURES = 32;
static const size_t MAX_TEXTURES_USING_PALETTE = 32;
static const size_t MAX_PALETTES = 128; // 256 color palettes only
static const size_t MAX_PALETTE_REMAPS = 128; // 256 entries only
static const size_t MAX_TRANSFORM_MATRIXES = 1024;
static const size_t MAX_RENDER_COUNT = 524288; // 0x00080000
static const float MAX_RENDER_DEPTH = 1.0f;
static const float RENDER_DEPTH_DELTA = ::MAX_RENDER_DEPTH / ::MAX_RENDER_COUNT;

static std::array<ID3D11ShaderResourceView*, ::MAX_TEXTURES + ::MAX_TEXTURES_USING_PALETTE + 2 /* palette + remap */>  NULL_TEXTURES{};

static std::array<uint8_t, ff::constants::palette_size> DEFAULT_PALETTE_REMAP =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
};

static size_t DEFAULT_PALETTE_REMAP_HASH = ff::stable_hash_bytes(::DEFAULT_PALETTE_REMAP.data(), ff::array_byte_size(::DEFAULT_PALETTE_REMAP));

namespace
{
    enum class alpha_type
    {
        opaque,
        transparent,
        invisible,
    };

    enum class last_depth_type
    {
        none,
        nudged,

        line,
        circle,
        triangle,
        sprite,

        line_no_overlap,
        circle_no_overlap,
        triangle_no_overlap,
        sprite_no_overlap,

        start_no_overlap = line_no_overlap,
    };

    enum class geometry_bucket_type
    {
        lines,
        circles,
        triangles,
        sprites,
        palette_sprites,

        lines_alpha,
        circles_alpha,
        triangles_alpha,
        sprites_alpha,

        count,
        first_alpha = lines_alpha,
    };

    class geometry_bucket
    {
    private:
        geometry_bucket(
            ::geometry_bucket_type bucket_type,
            const std::type_info& item_type,
            size_t item_size,
            size_t item_align,
            const D3D11_INPUT_ELEMENT_DESC* element_desc,
            size_t element_count)
            : bucket_type_(bucket_type)
            , item_type_(&item_type)
            , item_size_(item_size)
            , item_align(item_align)
            , render_start_(0)
            , render_count_(0)
            , data_start(nullptr)
            , data_cur(nullptr)
            , data_end(nullptr)
            , element_desc(element_desc)
            , element_count(element_count)
        {}

    public:
        template<typename T, ::geometry_bucket_type BucketType>
        static ::geometry_bucket create()
        {
            return ::geometry_bucket(BucketType, typeid(T), sizeof(T), alignof(T), T::layout().data(), T::layout().size());
        }

        geometry_bucket(::geometry_bucket&& rhs) noexcept
            : vs_res(std::move(rhs.vs_res))
            , gs_res(std::move(rhs.gs_res))
            , ps_res(std::move(rhs.ps_res))
            , ps_palette_out_res(std::move(rhs.ps_palette_out_res))
            , layout(std::move(rhs.layout))
            , vs(std::move(rhs.vs))
            , gs(std::move(rhs.gs))
            , ps(std::move(rhs.ps))
            , ps_palette_out(std::move(rhs.ps_palette_out))
            , element_desc(rhs.element_desc)
            , element_count(rhs.element_count)
            , bucket_type_(rhs.bucket_type_)
            , item_type_(rhs.item_type_)
            , item_size_(rhs.item_size_)
            , item_align(rhs.item_align)
            , render_start_(0)
            , render_count_(0)
            , data_start(rhs.data_start)
            , data_cur(rhs.data_cur)
            , data_end(rhs.data_end)
        {
            rhs.data_start = nullptr;
            rhs.data_cur = nullptr;
            rhs.data_end = nullptr;
        }

        ~geometry_bucket()
        {
            ::_aligned_free(this->data_start);
        }

        void reset(std::string_view vs_res, std::string_view gs_res, std::string_view ps_res, std::string_view ps_palette_out_res)
        {
            this->reset();

            this->vs_res = ff::global_resources::get(vs_res);
            this->gs_res = ff::global_resources::get(gs_res);
            this->ps_res = ff::global_resources::get(ps_res);
            this->ps_palette_out_res = ff::global_resources::get(ps_palette_out_res);
        }

        void reset()
        {
            this->layout.Reset();
            this->vs.Reset();
            this->gs.Reset();
            this->ps.Reset();
            this->ps_palette_out.Reset();

            ::_aligned_free(this->data_start);
            this->data_start = nullptr;
            this->data_cur = nullptr;
            this->data_end = nullptr;
        }

        void apply(ID3D11Buffer* geometry_buffer, bool palette_out) const
        {
            const_cast<::geometry_bucket*>(this)->create_shaders(palette_out);

            ff::dx11::device_state& state = ff::dx11::get_device_state();
            state.set_vertex_ia(geometry_buffer, item_size(), 0);
            state.set_layout_ia(this->layout.Get());
            state.set_vs(this->vs.Get());
            state.set_gs(this->gs.Get());
            state.set_ps(palette_out ? this->ps_palette_out.Get() : this->ps.Get());
        }

        void* add(const void* data = nullptr)
        {
            if (this->data_cur == this->data_end)
            {
                size_t cur_size = this->data_end - this->data_start;
                size_t new_size = std::max<size_t>(cur_size * 2, this->item_size_ * 64);
                this->data_start = reinterpret_cast<uint8_t*>(_aligned_realloc(this->data_start, new_size, this->item_align));
                this->data_cur = this->data_start + cur_size;
                this->data_end = this->data_start + new_size;
            }

            if (this->data_cur && data)
            {
                std::memcpy(this->data_cur, data, this->item_size_);
            }

            void* result = this->data_cur;
            this->data_cur += this->item_size_;
            return result;
        }

        size_t item_size() const
        {
            return this->item_size_;
        }

        const std::type_info& item_type() const
        {
            return *this->item_type_;
        }

        ::geometry_bucket_type bucket_type() const
        {
            return this->bucket_type_;
        }

        size_t count() const
        {
            return (this->data_cur - this->data_start) / this->item_size_;
        }

        void clear_items()
        {
            this->data_cur = this->data_start;
        }

        size_t byte_size() const
        {
            return this->data_cur - this->data_start;
        }

        const uint8_t* data() const
        {
            return this->data_start;
        }

        void render_start(size_t start)
        {
            this->render_start_ = start;
            this->render_count_ = this->count();
        }

        size_t render_start() const
        {
            return this->render_start_;
        }

        size_t render_count() const
        {
            return this->render_count_;
        }

    private:
        void create_shaders(bool palette_out)
        {
            if (!this->vs)
            {
                this->vs = ff::dx11::get_object_cache().get_vertex_shader_and_input_layout(this->vs_res.resource()->name(), this->layout, this->element_desc, this->element_count);
            }

            if (!this->gs)
            {
                this->gs = ff::dx11::get_object_cache().get_geometry_shader(this->gs_res.resource()->name());
            }

            Microsoft::WRL::ComPtr<ID3D11PixelShader>& ps = palette_out ? this->ps_palette_out : this->ps;
            if (!ps)
            {
                ps = ff::dx11::get_object_cache().get_pixel_shader(palette_out ? this->ps_palette_out_res.resource()->name() : this->ps_res.resource()->name());
            }
        }

        ff::auto_resource<ff::shader> vs_res;
        ff::auto_resource<ff::shader> gs_res;
        ff::auto_resource<ff::shader> ps_res;
        ff::auto_resource<ff::shader> ps_palette_out_res;

        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_palette_out;

        const D3D11_INPUT_ELEMENT_DESC* element_desc;
        size_t element_count;

        ::geometry_bucket_type bucket_type_;
        const std::type_info* item_type_;
        size_t item_size_;
        size_t item_align;
        size_t render_start_;
        size_t render_count_;
        uint8_t* data_start;
        uint8_t* data_cur;
        uint8_t* data_end;
    };

    struct alpha_geometry_entry
    {
        const ::geometry_bucket* bucket;
        size_t index;
        float depth;
    };

    struct geometry_shader_constants_0
    {
        DirectX::XMFLOAT4X4 projection;
        ff::point_float view_size;
        ff::point_float view_scale;
        float z_offset;
        float padding[3];
    };

    struct geometry_shader_constants_1
    {
        std::vector<DirectX::XMFLOAT4X4> model;
    };

    struct pixel_shader_constants_0
    {
        std::array<ff::rect_float, ::MAX_TEXTURES_USING_PALETTE> texture_palette_sizes;
    };
}

namespace std
{
    static bool operator==(const DirectX::XMFLOAT4X4& lhs, const DirectX::XMFLOAT4X4& rhs)
    {
        return ::operator==(lhs, rhs);
    }
}

static ::alpha_type get_alpha_type(const DirectX::XMFLOAT4& color, bool force_opaque)
{
    if (color.w == 0)
    {
        return alpha_type::invisible;
    }

    if (color.w == 1 || force_opaque)
    {
        return alpha_type::opaque;
    }

    return alpha_type::transparent;
}

static alpha_type get_alpha_type(const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    alpha_type type = alpha_type::invisible;

    for (size_t i = 0; i < count; i++)
    {
        switch (::get_alpha_type(colors[i], force_opaque))
        {
            case alpha_type::opaque:
                type = alpha_type::opaque;
                break;

            case alpha_type::transparent:
                return alpha_type::transparent;
        }
    }

    return type;
}

static alpha_type get_alpha_type(const ff::sprite_data& data, const DirectX::XMFLOAT4& color, bool force_opaque)
{
    switch (::get_alpha_type(color, force_opaque))
    {
        case alpha_type::transparent:
            return ff::flags::has(data.type(), ff::sprite_type::palette) ? alpha_type::opaque : alpha_type::transparent;

        case alpha_type::opaque:
            return (ff::flags::has(data.type(), ff::sprite_type::transparent) && !force_opaque)
                ? alpha_type::transparent
                : alpha_type::opaque;

        default:
            return alpha_type::invisible;
    }
}

static alpha_type get_alpha_type(const ff::sprite_data** datas, const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    alpha_type type = alpha_type::invisible;

    for (size_t i = 0; i < count; i++)
    {
        switch (::get_alpha_type(*datas[i], colors[i], force_opaque))
        {
            case alpha_type::opaque:
                type = alpha_type::opaque;
                break;

            case alpha_type::transparent:
                return alpha_type::transparent;
        }
    }

    return type;
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

static ff::rect_float get_rotated_view_rect(ff::target_base& target, const ff::rect_float& view_rect)
{
    ff::window_size size = target.size();
    ff::rect_float rotated_view_rect;

    switch (size.current_rotation)
    {
        default:
            rotated_view_rect = view_rect;
            break;

        case DMDO_90:
            {
                float height = size.rotated_pixel_size().cast<float>().y;
                rotated_view_rect.left = height - view_rect.bottom;
                rotated_view_rect.top = view_rect.left;
                rotated_view_rect.right = height - view_rect.top;
                rotated_view_rect.bottom = view_rect.right;
            } break;

        case DMDO_180:
            {
                ff::point_float target_size = size.rotated_pixel_size().cast<float>();
                rotated_view_rect.left = target_size.x - view_rect.right;
                rotated_view_rect.top = target_size.y - view_rect.bottom;
                rotated_view_rect.right = target_size.x - view_rect.left;
                rotated_view_rect.bottom = target_size.y - view_rect.top;
            } break;

        case DMDO_270:
            {
                float width = size.rotated_pixel_size().cast<float>().x;
                rotated_view_rect.left = view_rect.top;
                rotated_view_rect.top = width - view_rect.right;
                rotated_view_rect.right = view_rect.bottom;
                rotated_view_rect.bottom = width - view_rect.left;
            } break;
    }

    return rotated_view_rect;
}

static DirectX::XMMATRIX get_view_matrix(const ff::rect_float& world_rect)
{
    return DirectX::XMMatrixOrthographicOffCenterLH(
        world_rect.left,
        world_rect.right,
        world_rect.bottom,
        world_rect.top,
        0, ::MAX_RENDER_DEPTH);
}

static DirectX::XMMATRIX get_orientation_matrix(ff::target_base& target, const ff::rect_float& view_rect, ff::point_float world_center)
{
    DirectX::XMMATRIX orientation_matrix;

    int degrees = target.size().current_rotation;
    switch (degrees)
    {
        default:
            orientation_matrix = DirectX::XMMatrixIdentity();
            break;

        case DMDO_90:
        case DMDO_270:
            {
                float view_height_per_width = view_rect.height() / view_rect.width();
                float view_width_per_height = view_rect.width() / view_rect.height();

                orientation_matrix =
                    DirectX::XMMatrixTransformation2D(
                        DirectX::XMVectorSet(world_center.x, world_center.y, 0, 0), 0, // scale center
                        DirectX::XMVectorSet(view_height_per_width, view_width_per_height, 1, 1), // scale
                        DirectX::XMVectorSet(world_center.x, world_center.y, 0, 0), // rotation center
                        ff::math::pi<float>() * degrees / 2.0f, // rotation
                        DirectX::XMVectorZero()); // translation
            } break;

        case DMDO_180:
            orientation_matrix =
                DirectX::XMMatrixAffineTransformation2D(
                    DirectX::XMVectorSet(1, 1, 1, 1), // scale
                    DirectX::XMVectorSet(world_center.x, world_center.y, 0, 0), // rotation center
                    ff::math::pi<float>(), // rotation
                    DirectX::XMVectorZero()); // translation
            break;
    }

    return orientation_matrix;
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

static bool setup_view_matrix(ff::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect, DirectX::XMFLOAT4X4& view_matrix)
{
    if (world_rect.width() != 0 && world_rect.height() != 0 && view_rect.width() > 0 && view_rect.height() > 0)
    {
        DirectX::XMMATRIX unoriented_view_matrix = ::get_view_matrix(world_rect);
        DirectX::XMMATRIX orientation_matrix = ::get_orientation_matrix(target, view_rect, world_rect.center());
        DirectX::XMStoreFloat4x4(&view_matrix, DirectX::XMMatrixTranspose(orientation_matrix * unoriented_view_matrix));

        return true;
    }

    return false;
}

static bool setup_render_target(ff::target_base& target, ff::depth* depth, const ff::rect_float& view_rect)
{
    ID3D11RenderTargetView* target_view = target.view();
    if (target_view)
    {
        ID3D11DepthStencilView* depth_view = nullptr;
        if (depth)
        {
            if (depth->size(target.size().pixel_size))
            {
                depth->clear(0, 0);
                depth_view = depth->view();
            }

            if (!depth_view)
            {
                assert(false);
                return false;
            }
        }

        ff::rect_float rotated_view_rect = ::get_rotated_view_rect(target, view_rect);
        D3D11_VIEWPORT viewport = ::get_viewport(rotated_view_rect);

        ff::dx11::get_device_state().set_targets(&target_view, 1, depth_view);
        ff::dx11::get_device_state().set_viewports(&viewport, 1);

        return true;
    }

    assert(false);
    return false;
}

namespace
{
    class draw_device_internal
        : public ff::draw_device
        , public ff::draw_base
        , public ff_internal_dx::device_child_base
    {
    public:
        draw_device_internal()
            : state(::draw_device_internal::state_t::invalid)
            , world_matrix_stack_changing_connection(this->world_matrix_stack_.matrix_changing().connect(std::bind(&draw_device_internal::matrix_changing, this, std::placeholders::_1)))
            , geometry_buffer(D3D11_BIND_VERTEX_BUFFER)
            , geometry_constants_buffer_0(D3D11_BIND_CONSTANT_BUFFER)
            , geometry_constants_buffer_1(D3D11_BIND_CONSTANT_BUFFER)
            , pixel_constants_buffer_0(D3D11_BIND_CONSTANT_BUFFER)
            , geometry_buckets
        {
            ::geometry_bucket::create<ff::vertex::line_geometry, ::geometry_bucket_type::lines>(),
            ::geometry_bucket::create<ff::vertex::circle_geometry, ::geometry_bucket_type::circles>(),
            ::geometry_bucket::create<ff::vertex::triangle_geometry, ::geometry_bucket_type::triangles>(),
            ::geometry_bucket::create<ff::vertex::sprite_geometry, ::geometry_bucket_type::sprites>(),
            ::geometry_bucket::create<ff::vertex::sprite_geometry, ::geometry_bucket_type::palette_sprites>(),

            ::geometry_bucket::create<ff::vertex::line_geometry, ::geometry_bucket_type::lines_alpha>(),
            ::geometry_bucket::create<ff::vertex::circle_geometry, ::geometry_bucket_type::circles_alpha>(),
            ::geometry_bucket::create<ff::vertex::triangle_geometry, ::geometry_bucket_type::triangles_alpha>(),
            ::geometry_bucket::create<ff::vertex::sprite_geometry, ::geometry_bucket_type::sprites_alpha>(),
        }
        {
            this->reset();

            ff_internal_dx::add_device_child(this, ff_internal_dx::device_reset_priority::normal);
        }

        virtual ~draw_device_internal() override
        {
            assert(this->state != ::draw_device_internal::state_t::drawing);
            ff_internal_dx::remove_device_child(this);
        }

        draw_device_internal(draw_device_internal&& other) noexcept = delete;
        draw_device_internal(const draw_device_internal& other) = delete;
        draw_device_internal& operator=(draw_device_internal&& other) noexcept = delete;
        draw_device_internal& operator=(const draw_device_internal& other) = delete;

        virtual bool valid() const override
        {
            return this->state != ::draw_device_internal::state_t::invalid;
        }

        // graphics_child_base
        virtual bool reset()
        {
            this->destroy();

            // Geometry buckets
            this->get_geometry_bucket(::geometry_bucket_type::lines).reset("ff.shader.line_vs", "ff.shader.line_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::circles).reset("ff.shader.circle_vs", "ff.shader.circle_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::triangles).reset("ff.shader.triangle_vs", "ff.shader.triangle_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::sprites).reset("ff.shader.sprite_vs", "ff.shader.sprite_gs", "ff.shader.sprite_ps", "ff.shader.palette_out_sprite_ps");
            this->get_geometry_bucket(::geometry_bucket_type::palette_sprites).reset("ff.shader.sprite_vs", "ff.shader.sprite_gs", "ff.shader.sprite_palette_ps", "ff.shader.palette_out_sprite_palette_ps");

            this->get_geometry_bucket(::geometry_bucket_type::lines_alpha).reset("ff.shader.line_vs", "ff.shader.line_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::circles_alpha).reset("ff.shader.circle_vs", "ff.shader.circle_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::triangles_alpha).reset("ff.shader.triangle_vs", "ff.shader.triangle_gs", "ff.shader.color_ps", "ff.shader.palette_out_color_ps");
            this->get_geometry_bucket(::geometry_bucket_type::sprites_alpha).reset("ff.shader.sprite_vs", "ff.shader.sprite_gs", "ff.shader.sprite_ps", "ff.shader.palette_out_sprite_ps");

            // Palette
            this->palette_stack.push_back(nullptr);
            this->palette_texture = std::make_shared<ff::texture>(
                ff::point_size(ff::constants::palette_size, ::MAX_PALETTES).cast<int>(), ff::dxgi::PALETTE_FORMAT);

            this->palette_remap_stack.push_back(std::make_pair(::DEFAULT_PALETTE_REMAP.data(), ::DEFAULT_PALETTE_REMAP_HASH));
            this->palette_remap_texture = std::make_shared<ff::texture>(
                ff::point_size(ff::constants::palette_size, ::MAX_PALETTE_REMAPS).cast<int>(), ff::dxgi::PALETTE_INDEX_FORMAT);

            // States
            this->sampler_stack.push_back(::get_texture_sampler_state(D3D11_FILTER_MIN_MAG_MIP_POINT));
            this->opaque_state = ::create_opaque_draw_state();
            this->alpha_state = ::create_alpha_draw_state();
            this->pre_multiplied_alpha_state = ::create_pre_multiplied_alpha_draw_state();

            this->state = ::draw_device_internal::state_t::valid;
            return true;
        }

        virtual ff::draw_ptr begin_draw(ff::target_base& target, ff::depth* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::draw_options options) override
        {
            this->end_draw();

            if (::setup_view_matrix(target, view_rect, world_rect, this->view_matrix) &&
                ::setup_render_target(target, depth, view_rect))
            {
                this->init_geometry_constant_buffers_0(target, view_rect, world_rect);
                this->target_requires_palette = ff::dxgi::palette_format(target.format());
                this->force_pre_multiplied_alpha = ff::flags::has(options, ff::draw_options::pre_multiplied_alpha) && ff::dxgi::supports_pre_multiplied_alpha(target.format()) ? 1 : 0;
                this->state = ::draw_device_internal::state_t::drawing;

                return this;
            }

            return nullptr;
        }

        virtual void end_draw() override
        {
            if (this->state == state_t::drawing)
            {
                this->flush();

                ff::dx11::get_device_state().set_resources_ps(::NULL_TEXTURES.data(), 0, ::NULL_TEXTURES.size());

                this->state = ::draw_device_internal::state_t::valid;
                this->palette_stack.resize(1);
                this->palette_remap_stack.resize(1);
                this->sampler_stack.resize(1);
                this->custom_context_stack.clear();
                this->world_matrix_stack_.reset();
                this->draw_depth = 0;
                this->force_no_overlap = 0;
                this->force_opaque = 0;
                this->force_pre_multiplied_alpha = 0;
            }
        }

        virtual void draw_sprite(const ff::sprite_data& sprite, const ff::transform& transform) override
        {
            ::alpha_type alpha_type = ::get_alpha_type(sprite, transform.color, this->force_opaque || this->target_requires_palette);
            if (alpha_type != ::alpha_type::invisible && sprite.view())
            {
                bool use_palette = ff::flags::has(sprite.type(), ff::sprite_type::palette);
                ::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent && !this->target_requires_palette)
                    ? (use_palette ? ::geometry_bucket_type::palette_sprites : ::geometry_bucket_type::sprites_alpha)
                    : (use_palette ? ::geometry_bucket_type::palette_sprites : ::geometry_bucket_type::sprites);

                float depth = this->nudge_depth(this->force_no_overlap ? ::last_depth_type::sprite_no_overlap : ::last_depth_type::sprite);
                ff::vertex::sprite_geometry& input = *reinterpret_cast<ff::vertex::sprite_geometry*>(this->add_geometry(nullptr, bucket_type, depth));

                this->get_world_matrix_and_texture_index(*sprite.view(), use_palette, input.matrix_index, input.texture_index);
                input.position.x = transform.position.x;
                input.position.y = transform.position.y;
                input.position.z = depth;
                input.scale = *reinterpret_cast<const DirectX::XMFLOAT2*>(&transform.scale);
                input.rotate = transform.rotation_radians();
                input.color = transform.color;
                input.uv_rect = *reinterpret_cast<const DirectX::XMFLOAT4*>(&sprite.texture_uv());
                input.rect = *reinterpret_cast<const DirectX::XMFLOAT4*>(&sprite.world());
            }
        }

        virtual void draw_line_strip(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count, float thickness, bool pixel_thickness) override
        {
            this->draw_line_strip(points, count, colors, count, thickness, pixel_thickness);
        }

        virtual void draw_line_strip(const ff::point_float* points, size_t count, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override
        {
            this->draw_line_strip(points, count, &color, 1, thickness, pixel_thickness);
        }

        virtual void draw_line(const ff::point_float& start, const ff::point_float& end, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override
        {
            const ff::point_float points[2] = { start, end };
            this->draw_line_strip(points, 2, color, thickness, pixel_thickness);
        }

        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4* colors) override
        {
            const float tri_points[12] =
            {
                rect.left, rect.top,
                rect.right, rect.top,
                rect.right, rect.bottom,
                rect.right, rect.bottom,
                rect.left, rect.bottom,
                rect.left, rect.top,
            };

            const DirectX::XMFLOAT4 tri_colors[6] =
            {
                colors[0],
                colors[1],
                colors[2],
                colors[2],
                colors[3],
                colors[0],
            };

            this->draw_filled_triangles(reinterpret_cast<const ff::point_float*>(tri_points), tri_colors, 2);
        }

        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color) override
        {
            const float tri_points[12] =
            {
                rect.left, rect.top,
                rect.right, rect.top,
                rect.right, rect.bottom,
                rect.right, rect.bottom,
                rect.left, rect.bottom,
                rect.left, rect.top,
            };

            const DirectX::XMFLOAT4 tri_colors[6] =
            {
                color,
                color,
                color,
                color,
                color,
                color,
            };

            this->draw_filled_triangles(reinterpret_cast<const ff::point_float*>(tri_points), tri_colors, 2);
        }

        virtual void draw_filled_triangles(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count) override
        {
            ff::vertex::triangle_geometry input;
            input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
            input.depth = this->nudge_depth(this->force_no_overlap ? ::last_depth_type::triangle_no_overlap : ::last_depth_type::triangle);

            for (size_t i = 0; i < count; i++, points += 3, colors += 3)
            {
                std::memcpy(input.position, points, sizeof(input.position));
                std::memcpy(input.color, colors, sizeof(input.color));

                ::alpha_type alpha_type = ::get_alpha_type(colors, 3, this->force_opaque || this->target_requires_palette);
                if (alpha_type != ::alpha_type::invisible)
                {
                    ::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ::geometry_bucket_type::triangles_alpha : ::geometry_bucket_type::triangles;
                    this->add_geometry(&input, bucket_type, input.depth);
                }
            }
        }

        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color) override
        {
            this->draw_outline_circle(center, radius, color, color, std::abs(radius), false);
        }

        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color) override
        {
            this->draw_outline_circle(center, radius, inside_color, outside_color, std::abs(radius), false);
        }

        virtual void draw_outline_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override
        {
            ff::rect_float rect2 = rect.normalize();

            if (thickness < 0)
            {
                ff::point_float deflate = this->geometry_constants_0.view_scale * thickness;
                rect2 = rect2.deflate(deflate);
                this->draw_outline_rectangle(rect2, color, -thickness, pixel_thickness);
            }
            else if (!pixel_thickness && (thickness * 2 >= rect2.width() || thickness * 2 >= rect2.height()))
            {
                this->draw_filled_rectangle(rect2, color);
            }
            else
            {
                ff::point_float half_thickness(thickness / 2, thickness / 2);
                if (pixel_thickness)
                {
                    half_thickness *= this->geometry_constants_0.view_scale;
                }

                rect2 = rect2.deflate(half_thickness);

                const ff::point_float points[5] =
                {
                    rect2.top_left(),
                    rect2.top_right(),
                    rect2.bottom_right(),
                    rect2.bottom_left(),
                    rect2.top_left(),
                };

                this->draw_line_strip(points, 5, color, thickness, pixel_thickness);
            }
        }

        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override
        {
            this->draw_outline_circle(center, radius, color, color, thickness, pixel_thickness);
        }

        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color, float thickness, bool pixel_thickness) override
        {
            ff::vertex::circle_geometry input;
            input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
            input.position.x = center.x;
            input.position.y = center.y;
            input.position.z = this->nudge_depth(this->force_no_overlap ? ::last_depth_type::circle_no_overlap : ::last_depth_type::circle);
            input.radius = std::abs(radius);
            input.thickness = pixel_thickness ? -std::abs(thickness) : std::min(std::abs(thickness), input.radius);
            input.inside_color = inside_color;
            input.outside_color = outside_color;

            ::alpha_type alpha_type = ::get_alpha_type(&input.inside_color, 2, this->force_opaque || this->target_requires_palette);
            if (alpha_type != ::alpha_type::invisible)
            {
                ::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ::geometry_bucket_type::circles_alpha : ::geometry_bucket_type::circles;
                this->add_geometry(&input, bucket_type, input.position.z);
            }
        }

        virtual void draw_palette_line_strip(const ff::point_float* points, const int* colors, size_t count, float thickness, bool pixel_thickness) override
        {
            ff::stack_vector<DirectX::XMFLOAT4, 64> colors2;
            colors2.resize(count);

            for (size_t i = 0; i != colors2.size(); i++)
            {
                ff::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
            }

            this->draw_line_strip(points, count, colors2.data(), count, thickness, pixel_thickness);
        }

        virtual void draw_palette_line_strip(const ff::point_float* points, size_t count, int color, float thickness, bool pixel_thickness) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_line_strip(points, count, &color2, 1, thickness, pixel_thickness);
        }

        virtual void draw_palette_line(const ff::point_float& start, const ff::point_float& end, int color, float thickness, bool pixel_thickness) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_line(start, end, color2, thickness, pixel_thickness);
        }

        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, const int* colors) override
        {
            std::array<DirectX::XMFLOAT4, 4> colors2;

            for (size_t i = 0; i != colors2.size(); i++)
            {
                ff::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
            }

            this->draw_filled_rectangle(rect, colors2.data());
        }

        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, int color) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_filled_rectangle(rect, color2);
        }

        virtual void draw_palette_filled_triangles(const ff::point_float* points, const int* colors, size_t count) override
        {
            ff::stack_vector<DirectX::XMFLOAT4, 64 * 3> colors2;
            colors2.resize(count * 3);

            for (size_t i = 0; i != colors2.size(); i++)
            {
                ff::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
            }

            this->draw_filled_triangles(points, colors2.data(), count);
        }

        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int color) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_filled_circle(center, radius, color2);
        }

        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int inside_color, int outside_color) override
        {
            DirectX::XMFLOAT4 inside_color2, outside_color2;
            ff::palette_index_to_color(this->remap_palette_index(inside_color), inside_color2);
            ff::palette_index_to_color(this->remap_palette_index(outside_color), outside_color2);
            this->draw_filled_circle(center, radius, inside_color2, outside_color2);
        }

        virtual void draw_palette_outline_rectangle(const ff::rect_float& rect, int color, float thickness, bool pixel_thickness) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_outline_rectangle(rect, color2, thickness, pixel_thickness);
        }

        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int color, float thickness, bool pixel_thickness) override
        {
            DirectX::XMFLOAT4 color2;
            ff::palette_index_to_color(this->remap_palette_index(color), color2);
            this->draw_outline_circle(center, radius, color2, thickness, pixel_thickness);
        }

        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int inside_color, int outside_color, float thickness, bool pixel_thickness) override
        {
            DirectX::XMFLOAT4 inside_color2, outside_color2;
            ff::palette_index_to_color(this->remap_palette_index(inside_color), inside_color2);
            ff::palette_index_to_color(this->remap_palette_index(outside_color), outside_color2);
            this->draw_outline_circle(center, radius, inside_color2, outside_color2, thickness, pixel_thickness);
        }

        virtual ff::matrix_stack& world_matrix_stack() override
        {
            return this->world_matrix_stack_;
        }

        virtual void nudge_depth() override
        {
            this->last_depth_type = ::last_depth_type::nudged;
        }

        virtual void push_palette(ff::palette_base* palette) override
        {
            assert(!this->target_requires_palette && palette);
            this->palette_stack.push_back(palette);
            this->palette_index = ff::constants::invalid_dword;

            this->push_palette_remap(palette->index_remap(), palette->index_remap_hash());
        }

        virtual void pop_palette() override
        {
            assert(this->palette_stack.size() > 1);
            this->palette_stack.pop_back();
            this->palette_index = ff::constants::invalid_dword;

            this->pop_palette_remap();
        }

        virtual void push_palette_remap(const uint8_t* remap, size_t hash) override
        {
            this->palette_remap_stack.push_back(std::make_pair(
                remap ? remap : ::DEFAULT_PALETTE_REMAP.data(),
                remap ? (hash ? hash : ff::stable_hash_bytes(remap, ff::constants::palette_size)) : ::DEFAULT_PALETTE_REMAP_HASH));
            this->palette_remap_index = ff::constants::invalid_dword;
        }

        virtual void pop_palette_remap() override
        {
            assert(this->palette_remap_stack.size() > 1);
            this->palette_remap_stack.pop_back();
            this->palette_remap_index = ff::constants::invalid_dword;
        }

        virtual void push_no_overlap() override
        {
            this->force_no_overlap++;
        }

        virtual void pop_no_overlap() override
        {
            assert(this->force_no_overlap > 0);

            if (!--this->force_no_overlap)
            {
                this->nudge_depth();
            }
        }

        virtual void push_opaque() override
        {
            this->force_opaque++;
        }

        virtual void pop_opaque() override
        {
            assert(this->force_opaque > 0);
            this->force_opaque--;
        }

        virtual void push_pre_multiplied_alpha() override
        {
            if (!this->force_pre_multiplied_alpha)
            {
                this->flush();
            }

            this->force_pre_multiplied_alpha++;
        }

        virtual void pop_pre_multiplied_alpha() override
        {
            assert(this->force_pre_multiplied_alpha > 0);

            if (this->force_pre_multiplied_alpha == 1)
            {
                this->flush();
            }

            this->force_pre_multiplied_alpha--;
        }

        virtual void push_custom_context(ff::draw_base::custom_context_func&& func) override
        {
            this->flush();
            this->custom_context_stack.push_back(std::move(func));
        }

        virtual void pop_custom_context() override
        {
            assert(this->custom_context_stack.size());

            this->flush();
            this->custom_context_stack.pop_back();
        }

        virtual void push_sampler_linear_filter(bool linear_filter) override
        {
            this->flush();
            this->sampler_stack.push_back(::get_texture_sampler_state(linear_filter
                ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
                : D3D11_FILTER_MIN_MAG_MIP_POINT));
        }

        virtual void pop_sampler_linear_filter() override
        {
            assert(this->sampler_stack.size() > 1);

            this->flush();
            this->sampler_stack.pop_back();
        }

    private:
        void destroy()
        {
            this->state = ::draw_device_internal::state_t::invalid;

            this->geometry_constants_0 = ::geometry_shader_constants_0{};
            this->geometry_constants_1 = ::geometry_shader_constants_1{};
            this->pixel_constants_0 = ::pixel_shader_constants_0{};
            this->geometry_constants_hash_0 = 0;
            this->geometry_constants_hash_1 = 0;
            this->pixel_constants_hash_0 = 0;

            this->sampler_stack.clear();
            this->opaque_state = ff::dx11::fixed_state();
            this->alpha_state = ff::dx11::fixed_state();
            this->pre_multiplied_alpha_state = ff::dx11::fixed_state();
            this->custom_context_stack.clear();

            this->view_matrix = ff::matrix::identity_4x4();
            this->world_matrix_stack_.reset();
            this->world_matrix_to_index.clear();
            this->world_matrix_index = ff::constants::invalid_dword;

            std::memset(this->textures.data(), 0, ff::array_byte_size(this->textures));
            std::memset(this->textures_using_palette.data(), 0, ff::array_byte_size(this->textures_using_palette));
            this->texture_count = 0;
            this->textures_using_palette_count = 0;

            std::memset(this->palette_texture_hashes.data(), 0, ff::array_byte_size(this->palette_texture_hashes));
            this->palette_stack.clear();
            this->palette_to_index.clear();
            this->palette_texture = nullptr;
            this->palette_index = ff::constants::invalid_dword;

            std::memset(this->palette_remap_texture_hashes.data(), 0, ff::array_byte_size(this->palette_remap_texture_hashes));
            this->palette_remap_stack.clear();
            this->palette_remap_to_index.clear();
            this->palette_remap_texture = nullptr;
            this->palette_remap_index = ff::constants::invalid_dword;

            this->alpha_geometry.clear();
            this->last_depth_type = ::last_depth_type::none;
            this->draw_depth = 0;
            this->force_no_overlap = 0;
            this->force_opaque = 0;
            this->force_pre_multiplied_alpha = 0;

            for (auto& bucket : this->geometry_buckets)
            {
                bucket.reset();
            }
        }

        void matrix_changing(const ff::matrix_stack& matrix_stack)
        {
            this->world_matrix_index = ff::constants::invalid_dword;
        }

        void draw_line_strip(const ff::point_float* points, size_t point_count, const DirectX::XMFLOAT4* colors, size_t color_count, float thickness, bool pixel_thickness)
        {
            assert(color_count == 1 || color_count == point_count);
            thickness = pixel_thickness ? -std::abs(thickness) : std::abs(thickness);

            ff::vertex::line_geometry input;
            input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
            input.depth = this->nudge_depth(this->force_no_overlap ? ::last_depth_type::line_no_overlap : ::last_depth_type::line);
            input.color[0] = colors[0];
            input.color[1] = colors[0];
            input.thickness[0] = thickness;
            input.thickness[1] = thickness;

            const DirectX::XMFLOAT2* dxpoints = reinterpret_cast<const DirectX::XMFLOAT2*>(points);
            bool closed = point_count > 2 && points[0] == points[point_count - 1];
            ::alpha_type alpha_type = ::get_alpha_type(colors[0], this->force_opaque || this->target_requires_palette);

            for (size_t i = 0; i < point_count - 1; i++)
            {
                input.position[1] = dxpoints[i];
                input.position[2] = dxpoints[i + 1];

                input.position[0] = (i == 0)
                    ? (closed ? dxpoints[point_count - 2] : dxpoints[i])
                    : dxpoints[i - 1];

                input.position[3] = (i == point_count - 2)
                    ? (closed ? dxpoints[1] : dxpoints[i + 1])
                    : dxpoints[i + 2];

                if (color_count != 1)
                {
                    input.color[0] = colors[i];
                    input.color[1] = colors[i + 1];
                    alpha_type = ::get_alpha_type(colors + i, 2, this->force_opaque || this->target_requires_palette);
                }

                if (alpha_type != ::alpha_type::invisible)
                {
                    ::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ::geometry_bucket_type::lines_alpha : ::geometry_bucket_type::lines;
                    this->add_geometry(&input, bucket_type, input.depth);
                }
            }
        }

        void init_geometry_constant_buffers_0(ff::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect)
        {
            this->geometry_constants_0.view_size = view_rect.size() / static_cast<float>(target.size().dpi_scale);
            this->geometry_constants_0.view_scale = world_rect.size() / this->geometry_constants_0.view_size;
        }

        void update_geometry_constant_buffers_0()
        {
            this->geometry_constants_0.projection = this->view_matrix;

            size_t hash0 = ff::stable_hash_func(this->geometry_constants_0);
            if (!this->geometry_constants_hash_0 || this->geometry_constants_hash_0 != hash0)
            {
                this->geometry_constants_buffer_0.update_discard(&this->geometry_constants_0, sizeof(::geometry_shader_constants_0));
                this->geometry_constants_hash_0 = hash0;
            }
        }

        void update_geometry_constant_buffers_1()
        {
            // Build up model matrix array
            size_t world_matrix_count = this->world_matrix_to_index.size();
            this->geometry_constants_1.model.resize(world_matrix_count);

            for (const auto& iter : this->world_matrix_to_index)
            {
                this->geometry_constants_1.model[iter.second] = iter.first;
            }

            size_t hash1 = world_matrix_count
                ? ff::stable_hash_bytes(this->geometry_constants_1.model.data(), ff::vector_byte_size(this->geometry_constants_1.model))
                : 0;

            if (!this->geometry_constants_hash_1 || this->geometry_constants_hash_1 != hash1)
            {
                this->geometry_constants_hash_1 = hash1;
#if _DEBUG
                size_t buffer_size = sizeof(DirectX::XMFLOAT4X4) * ::MAX_TRANSFORM_MATRIXES;
#else
                size_t buffer_size = sizeof(DirectX::XMFLOAT4X4) * world_matrix_count;
#endif
                this->geometry_constants_buffer_1.update_discard(this->geometry_constants_1.model.data(), ff::vector_byte_size(this->geometry_constants_1.model), buffer_size);
            }
        }

        void update_pixel_constant_buffers_0()
        {
            if (this->textures_using_palette_count)
            {
                for (size_t i = 0; i < this->textures_using_palette_count; i++)
                {
                    ff::rect_float& rect = this->pixel_constants_0.texture_palette_sizes[i];
                    ff::point_float size = this->textures_using_palette[i]->view_texture()->size().cast<float>();
                    rect.left = size.x;
                    rect.top = size.y;
                }

                size_t hash0 = ff::stable_hash_func(this->pixel_constants_0);
                if (!this->pixel_constants_hash_0 || this->pixel_constants_hash_0 != hash0)
                {
                    this->pixel_constants_buffer_0.update_discard(&this->pixel_constants_0, sizeof(::pixel_shader_constants_0));
                    this->pixel_constants_hash_0 = hash0;
                }
            }
        }

        void update_palette_texture()
        {
            if (this->textures_using_palette_count && !this->palette_to_index.empty())
            {
                ID3D11Resource* dest_resource = this->palette_texture->dx_texture();
                CD3D11_BOX box(0, 0, 0, static_cast<int>(ff::constants::palette_size), 1, 1);

                for (const auto& iter : this->palette_to_index)
                {
                    ff::palette_base* palette = iter.second.first;
                    if (palette)
                    {
                        unsigned int index = iter.second.second;
                        size_t palette_row = palette->current_row();
                        const ff::palette_data* palette_data = palette->data();
                        size_t row_hash = palette_data->row_hash(palette_row);

                        if (this->palette_texture_hashes[index] != row_hash)
                        {
                            this->palette_texture_hashes[index] = row_hash;
                            ID3D11Resource* src_resource = palette_data->texture()->dx_texture();
                            box.top = static_cast<UINT>(palette_row);
                            box.bottom = box.top + 1;
                            ff::dx11::get_device_state().copy_subresource_region(dest_resource, 0, 0, index, 0, src_resource, 0, &box);
                        }
                    }
                }
            }

            if ((this->textures_using_palette_count || this->target_requires_palette) && !this->palette_remap_to_index.empty())
            {
                ID3D11Resource* dest_remap_resource = this->palette_remap_texture->dx_texture();
                CD3D11_BOX box(0, 0, 0, static_cast<int>(ff::constants::palette_size), 1, 1);

                for (const auto& iter : this->palette_remap_to_index)
                {
                    const uint8_t* remap = iter.second.first;
                    unsigned int row = iter.second.second;
                    size_t row_hash = iter.first;

                    if (this->palette_remap_texture_hashes[row] != row_hash)
                    {
                        this->palette_remap_texture_hashes[row] = row_hash;
                        box.top = row;
                        box.bottom = row + 1;
                        ff::dx11::get_device_state().update_subresource(dest_remap_resource, 0, &box, remap, static_cast<UINT>(ff::constants::palette_size), 0);
                    }
                }
            }
        }

        void set_shader_input()
        {
            std::array<ID3D11Buffer*, 2> buffers_gs = { this->geometry_constants_buffer_0.dx_buffer(), this->geometry_constants_buffer_1.dx_buffer() };
            ff::dx11::get_device_state().set_constants_gs(buffers_gs.data(), 0, buffers_gs.size());

            std::array<ID3D11Buffer*, 1> buffers_ps = { this->pixel_constants_buffer_0.dx_buffer() };
            ff::dx11::get_device_state().set_constants_ps(buffers_ps.data(), 0, buffers_ps.size());

            std::array<ID3D11SamplerState*, 1> sample_states = { this->sampler_stack.back().Get() };
            ff::dx11::get_device_state().set_samplers_ps(sample_states.data(), 0, sample_states.size());

            if (this->texture_count)
            {
                std::array<ID3D11ShaderResourceView*, ::MAX_TEXTURES> textures;
                for (size_t i = 0; i < this->texture_count; i++)
                {
                    textures[i] = this->textures[i]->view();
                }

                ff::dx11::get_device_state().set_resources_ps(textures.data(), 0, this->texture_count);
            }

            if (this->textures_using_palette_count)
            {
                std::array<ID3D11ShaderResourceView*, ::MAX_TEXTURES_USING_PALETTE> textures_using_palette;
                for (size_t i = 0; i < this->textures_using_palette_count; i++)
                {
                    textures_using_palette[i] = this->textures_using_palette[i]->view();
                }

                ff::dx11::get_device_state().set_resources_ps(textures_using_palette.data(), ::MAX_TEXTURES, this->textures_using_palette_count);
            }

            if (this->textures_using_palette_count || this->target_requires_palette)
            {
                std::array<ID3D11ShaderResourceView*, 2> palettes =
                {
                    (this->textures_using_palette_count ? this->palette_texture->view() : nullptr),
                    this->palette_remap_texture->view(),
                };

                ff::dx11::get_device_state().set_resources_ps(palettes.data(), ::MAX_TEXTURES + ::MAX_TEXTURES_USING_PALETTE, palettes.size());
            }
        }

        void flush()
        {
            if (this->last_depth_type != ::last_depth_type::none && this->create_geometry_buffer())
            {
                this->update_geometry_constant_buffers_0();
                this->update_geometry_constant_buffers_1();
                this->update_pixel_constant_buffers_0();
                this->update_palette_texture();
                this->set_shader_input();
                this->draw_opaque_geometry();
                this->draw_alpha_geometry();

                // Reset draw data

                this->world_matrix_to_index.clear();
                this->world_matrix_index = ff::constants::invalid_dword;

                this->palette_to_index.clear();
                this->palette_index = ff::constants::invalid_dword;
                this->palette_remap_to_index.clear();
                this->palette_remap_index = ff::constants::invalid_dword;

                this->texture_count = 0;
                this->textures_using_palette_count = 0;

                this->alpha_geometry.clear();
                this->last_depth_type = ::last_depth_type::none;
            }
        }

        bool create_geometry_buffer()
        {
            size_t byte_size = 0;

            for (::geometry_bucket& bucket : this->geometry_buckets)
            {
                byte_size = ff::math::round_up(byte_size, bucket.item_size());
                bucket.render_start(byte_size / bucket.item_size());
                byte_size += bucket.byte_size();
            }

            void* buffer_data = this->geometry_buffer.map(byte_size);
            if (buffer_data)
            {
                for (::geometry_bucket& bucket : this->geometry_buckets)
                {
                    if (bucket.render_count())
                    {
                        ::memcpy(reinterpret_cast<uint8_t*>(buffer_data) + bucket.render_start() * bucket.item_size(), bucket.data(), bucket.byte_size());
                        bucket.clear_items();
                    }
                }

                this->geometry_buffer.unmap();

                return true;
            }

            assert(false);
            return false;
        }

        void draw_opaque_geometry()
        {
            const ff::draw_base::custom_context_func* custom_func = this->custom_context_stack.size() ? &this->custom_context_stack.back() : nullptr;
            ff::dx11::get_device_state().set_topology_ia(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

            this->opaque_state.apply();

            for (::geometry_bucket& bucket : this->geometry_buckets)
            {
                if (bucket.bucket_type() >= ::geometry_bucket_type::first_alpha)
                {
                    break;
                }

                if (bucket.render_count())
                {
                    bucket.apply(this->geometry_buffer.dx_buffer(), this->target_requires_palette);

                    if (!custom_func || (*custom_func)(bucket.item_type(), true))
                    {
                        ff::dx11::get_device_state().draw(bucket.render_count(), bucket.render_start());
                    }
                }
            }
        }

        void draw_alpha_geometry()
        {
            const size_t alpha_geometry_size = this->alpha_geometry.size();
            if (alpha_geometry_size)
            {
                const ff::draw_base::custom_context_func* custom_func = this->custom_context_stack.size() ? &this->custom_context_stack.back() : nullptr;
                ff::dx11::get_device_state().set_topology_ia(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

                ff::dx11::fixed_state& alpha_state = this->force_pre_multiplied_alpha ? this->pre_multiplied_alpha_state : this->alpha_state;
                alpha_state.apply();

                for (size_t i = 0; i < alpha_geometry_size; )
                {
                    const ::alpha_geometry_entry& entry = this->alpha_geometry[i];
                    size_t geometry_count = 1;

                    for (i++; i < alpha_geometry_size; i++, geometry_count++)
                    {
                        const ::alpha_geometry_entry& entry2 = this->alpha_geometry[i];
                        if (entry2.bucket != entry.bucket ||
                            entry2.depth != entry.depth ||
                            entry2.index != entry.index + geometry_count)
                        {
                            break;
                        }
                    }

                    entry.bucket->apply(this->geometry_buffer.dx_buffer(), this->target_requires_palette);

                    if (!custom_func || (*custom_func)(entry.bucket->item_type(), false))
                    {
                        ff::dx11::get_device_state().draw(geometry_count, entry.bucket->render_start() + entry.index);
                    }
                }
            }
        }

        float nudge_depth(::last_depth_type depth_type)
        {
            if (depth_type < ::last_depth_type::start_no_overlap || this->last_depth_type != depth_type)
            {
                this->draw_depth += ::RENDER_DEPTH_DELTA;
            }

            this->last_depth_type = depth_type;
            return this->draw_depth;
        }

        unsigned int get_world_matrix_index()
        {
            unsigned int index = this->get_world_matrix_index_no_flush();
            if (index == ff::constants::invalid_dword)
            {
                this->flush();
                index = this->get_world_matrix_index_no_flush();
            }

            return index;
        }

        unsigned int get_world_matrix_index_no_flush()
        {
            if (this->world_matrix_index == ff::constants::invalid_dword)
            {
                DirectX::XMFLOAT4X4 wm;
                DirectX::XMStoreFloat4x4(&wm, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&this->world_matrix_stack_.matrix())));
                auto iter = this->world_matrix_to_index.find(wm);

                if (iter == this->world_matrix_to_index.cend() && this->world_matrix_to_index.size() != ::MAX_TRANSFORM_MATRIXES)
                {
                    iter = this->world_matrix_to_index.try_emplace(wm, static_cast<unsigned int>(this->world_matrix_to_index.size())).first;
                }

                if (iter != this->world_matrix_to_index.cend())
                {
                    this->world_matrix_index = iter->second;
                }
            }

            return this->world_matrix_index;
        }

        unsigned int get_texture_index_no_flush(const ff::texture_view_base& texture_view, bool use_palette)
        {
            if (use_palette)
            {
                unsigned int palette_index = (this->palette_index == ff::constants::invalid_dword) ? this->get_palette_index_no_flush() : this->palette_index;
                if (palette_index == ff::constants::invalid_dword)
                {
                    return ff::constants::invalid_dword;
                }

                unsigned int palette_remap_index = (this->palette_remap_index == ff::constants::invalid_dword) ? this->get_palette_remap_index_no_flush() : this->palette_remap_index;
                if (palette_remap_index == ff::constants::invalid_dword)
                {
                    return ff::constants::invalid_dword;
                }

                unsigned int texture_index = ff::constants::invalid_dword;

                for (size_t i = this->textures_using_palette_count; i != 0; i--)
                {
                    if (this->textures_using_palette[i - 1] == &texture_view)
                    {
                        texture_index = static_cast<unsigned int>(i - 1);
                        break;
                    }
                }

                if (texture_index == ff::constants::invalid_dword)
                {
                    if (this->textures_using_palette_count == ::MAX_TEXTURES_USING_PALETTE)
                    {
                        return ff::constants::invalid_dword;
                    }

                    this->textures_using_palette[this->textures_using_palette_count] = &texture_view;
                    texture_index = static_cast<unsigned int>(this->textures_using_palette_count++);
                }

                return texture_index | (palette_index << 8) | (palette_remap_index << 16);
            }
            else
            {
                unsigned int palette_remap_index = 0;
                if (this->target_requires_palette)
                {
                    palette_remap_index = (this->palette_remap_index == ff::constants::invalid_dword) ? this->get_palette_remap_index_no_flush() : this->palette_remap_index;
                    if (palette_remap_index == ff::constants::invalid_dword)
                    {
                        return ff::constants::invalid_dword;
                    }
                }

                unsigned int texture_index = ff::constants::invalid_dword;

                for (size_t i = this->texture_count; i != 0; i--)
                {
                    if (this->textures[i - 1] == &texture_view)
                    {
                        texture_index = static_cast<unsigned int>(i - 1);
                        break;
                    }
                }

                if (texture_index == ff::constants::invalid_dword)
                {
                    if (this->texture_count == ::MAX_TEXTURES)
                    {
                        return ff::constants::invalid_dword;
                    }

                    this->textures[this->texture_count] = &texture_view;
                    texture_index = static_cast<unsigned int>(this->texture_count++);
                }

                return texture_index | (palette_remap_index << 16);
            }
        }

        unsigned int get_palette_index_no_flush()
        {
            if (this->palette_index == ff::constants::invalid_dword)
            {
                if (this->target_requires_palette)
                {
                    // Not converting palette to RGBA, so don't use a palette
                    this->palette_index = 0;
                }
                else
                {
                    ff::palette_base* palette = this->palette_stack.back();
                    size_t palette_hash = palette ? palette->data()->row_hash(palette->current_row()) : 0;
                    auto iter = this->palette_to_index.find(palette_hash);

                    if (iter == this->palette_to_index.cend() && this->palette_to_index.size() != ::MAX_PALETTES)
                    {
                        iter = this->palette_to_index.try_emplace(palette_hash, std::make_pair(palette, static_cast<unsigned int>(this->palette_to_index.size()))).first;
                    }

                    if (iter != this->palette_to_index.cend())
                    {
                        this->palette_index = iter->second.second;
                    }
                }
            }

            return this->palette_index;
        }

        unsigned int get_palette_remap_index_no_flush()
        {
            if (this->palette_remap_index == ff::constants::invalid_dword)
            {
                auto& remap_pair = this->palette_remap_stack.back();
                auto iter = this->palette_remap_to_index.find(remap_pair.second);

                if (iter == this->palette_remap_to_index.cend() && this->palette_remap_to_index.size() != ::MAX_PALETTE_REMAPS)
                {
                    iter = this->palette_remap_to_index.try_emplace(remap_pair.second, std::make_pair(remap_pair.first, static_cast<unsigned int>(this->palette_remap_to_index.size()))).first;
                }

                if (iter != this->palette_remap_to_index.cend())
                {
                    this->palette_remap_index = iter->second.second;
                }
            }

            return this->palette_remap_index;
        }

        int remap_palette_index(int color) const
        {
            return this->palette_remap_stack.back().first[color];
        }

        void get_world_matrix_and_texture_index(const ff::texture_view_base& texture_view, bool use_palette, unsigned int& model_index, unsigned int& texture_index)
        {
            model_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index_no_flush() : this->world_matrix_index;
            texture_index = this->get_texture_index_no_flush(texture_view, use_palette);

            if (model_index == ff::constants::invalid_dword || texture_index == ff::constants::invalid_dword)
            {
                this->flush();
                this->get_world_matrix_and_texture_index(texture_view, use_palette, model_index, texture_index);
            }
        }

        void get_world_matrix_and_texture_indexes(ff::texture_view_base* const* texture_views, bool use_palette, unsigned int* texture_indexes, size_t count, unsigned int& model_index)
        {
            model_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index_no_flush() : this->world_matrix_index;
            bool flush = (model_index == ff::constants::invalid_dword);

            for (size_t i = 0; !flush && i < count; i++)
            {
                texture_indexes[i] = this->get_texture_index_no_flush(*texture_views[i], use_palette);
                flush |= (texture_indexes[i] == ff::constants::invalid_dword);
            }

            if (flush)
            {
                this->flush();
                this->get_world_matrix_and_texture_indexes(texture_views, use_palette, texture_indexes, count, model_index);
            }
        }

        void* add_geometry(const void* data, ::geometry_bucket_type bucket_type, float depth)
        {
            ::geometry_bucket& bucket = this->get_geometry_bucket(bucket_type);

            if (bucket_type >= ::geometry_bucket_type::first_alpha)
            {
                assert(!this->force_opaque);

                this->alpha_geometry.push_back(::alpha_geometry_entry
                    {
                        &bucket,
                        bucket.count(),
                        depth
                    });
            }

            return bucket.add(data);
        }

        ::geometry_bucket& get_geometry_bucket(::geometry_bucket_type type)
        {
            return this->geometry_buckets[static_cast<size_t>(type)];
        }

        enum class state_t
        {
            invalid,
            valid,
            drawing,
        } state;

        // Constant data for shaders
        ff::buffer geometry_buffer;
        ff::buffer geometry_constants_buffer_0;
        ff::buffer geometry_constants_buffer_1;
        ff::buffer pixel_constants_buffer_0;
        ::geometry_shader_constants_0 geometry_constants_0;
        ::geometry_shader_constants_1 geometry_constants_1;
        ::pixel_shader_constants_0 pixel_constants_0;
        size_t geometry_constants_hash_0;
        size_t geometry_constants_hash_1;
        size_t pixel_constants_hash_0;

        // Render state
        std::vector<Microsoft::WRL::ComPtr<ID3D11SamplerState>> sampler_stack;
        ff::dx11::fixed_state opaque_state;
        ff::dx11::fixed_state alpha_state;
        ff::dx11::fixed_state pre_multiplied_alpha_state;
        std::vector<ff::draw_base::custom_context_func> custom_context_stack;

        // Matrixes
        DirectX::XMFLOAT4X4 view_matrix;
        ff::matrix_stack world_matrix_stack_;
        ff::signal_connection world_matrix_stack_changing_connection;
        std::unordered_map<DirectX::XMFLOAT4X4, unsigned int, ff::stable_hash<DirectX::XMFLOAT4X4>> world_matrix_to_index;
        unsigned int world_matrix_index;

        // Textures
        std::array<const ff::texture_view_base*, ::MAX_TEXTURES> textures;
        std::array<const ff::texture_view_base*, ::MAX_TEXTURES_USING_PALETTE> textures_using_palette;
        size_t texture_count;
        size_t textures_using_palette_count;

        // Palettes
        bool target_requires_palette;

        std::vector<ff::palette_base*> palette_stack;
        std::shared_ptr<ff::texture> palette_texture;
        std::array<size_t, ::MAX_PALETTES> palette_texture_hashes;
        std::unordered_map<size_t, std::pair<ff::palette_base*, unsigned int>, ff::no_hash<size_t>> palette_to_index;
        unsigned int palette_index;

        std::vector<std::pair<const uint8_t*, size_t>> palette_remap_stack;
        std::shared_ptr<ff::texture> palette_remap_texture;
        std::array<size_t, ::MAX_PALETTE_REMAPS> palette_remap_texture_hashes;
        std::unordered_map<size_t, std::pair<const uint8_t*, unsigned int>, ff::no_hash<size_t>> palette_remap_to_index;
        unsigned int palette_remap_index;

        // Render data
        std::vector<::alpha_geometry_entry> alpha_geometry;
        std::array<::geometry_bucket, static_cast<size_t>(::geometry_bucket_type::count)> geometry_buckets;
        ::last_depth_type last_depth_type;
        float draw_depth;
        int force_no_overlap;
        int force_opaque;
        int force_pre_multiplied_alpha;
    };
}

std::unique_ptr<ff::draw_device> ff::draw_device::create()
{
    return std::make_unique<::draw_device_internal>();
}

ff::draw_ptr ff::draw_device::begin_draw(ff::target_base& target, ff::depth* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::draw_options options)
{
    return this->begin_draw(target, depth, std::floor(view_rect).cast<float>(), std::floor(world_rect).cast<float>(), options);
}

#endif
