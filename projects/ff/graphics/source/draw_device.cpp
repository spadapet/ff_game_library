#include "pch.h"
#include "draw_device.h"
#include "dx11_device_state.h"
#include "dx11_fixed_state.h"
#include "dx11_object_cache.h"
#include "dx11_texture_view_base.h"
#include "graphics.h"
#include "graphics_child_base.h"
#include "matrix_stack.h"
#include "palette_data.h"
#include "shader.h"

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
            geometry_bucket_type bucket_type,
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
        template<typename T, geometry_bucket_type BucketType>
        static geometry_bucket create()
        {
            return geometry_bucket(BucketType, typeid(T), sizeof(T), alignof(T), T::layout().data(), T::layout().size());
        }

        geometry_bucket(geometry_bucket&& rhs)
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

            this->vs_res = ff::resource_objects::global().get_resource_object(vs_res);
            this->gs_res = ff::resource_objects::global().get_resource_object(gs_res);
            this->ps_res = ff::resource_objects::global().get_resource_object(ps_res);
            this->ps_palette_out_res = ff::resource_objects::global().get_resource_object(ps_palette_out_res);
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
            const_cast<geometry_bucket*>(this)->create_shaders(palette_out);

            ff::dx11_device_state& state = ff::graphics::dx11_device_state();
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
                this->data_start = (uint8_t*)_aligned_realloc(this->data_start, new_size, this->item_align);
                this->data_cur = this->data_start + cur_size;
                this->data_end = this->data_start + new_size;
            }

            if (data)
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

        geometry_bucket_type bucket_type() const
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
                this->vs = ff::graphics::dx11_object_cache().get_vertex_shader_and_input_layout(
                    ff::resource_objects::global(),
                    this->vs_res.resource()->name(),
                    this->layout, this->element_desc, this->element_count);
            }

            if (!this->gs)
            {
                this->gs = ff::graphics::dx11_object_cache().get_geometry_shader(
                    ff::resource_objects::global(), this->gs_res.resource()->name());
            }

            Microsoft::WRL::ComPtr<ID3D11PixelShader>& ps = palette_out ? this->ps_palette_out : this->ps;
            if (!ps)
            {
                ps = ff::graphics::dx11_object_cache().get_pixel_shader(
                    ff::resource_objects::global(),
                    palette_out ? this->ps_palette_out_res.resource()->name() : this->ps_res.resource()->name());
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

        geometry_bucket_type bucket_type_;
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
        const geometry_bucket* bucket;
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

    class draw_device_internal
        : public ff::draw_device
        , public ff::draw_base
        , public ff::internal::graphics_child_base
    {
    public:
        draw_device_internal()
            : state(::draw_device_internal::state_t::invalid)
        {
            ff::graphics::internal::add_child(this);
        }

        virtual ~draw_device_internal() override
        {
            assert(this->state != ::draw_device_internal::state_t::drawing);
            ff::graphics::internal::remove_child(this);
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
        virtual bool reset();

        virtual ff::draw_ptr begin_draw(ff::dx11_target_base* target, ff::dx11_depth* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::draw_options options) override;
        virtual void end_draw() override;

        virtual void draw_sprite(const ff::sprite_data& sprite, const ff::transform& transform) override;
        virtual void draw_line_strip(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count, float thickness, bool pixel_thickness = false) override;
        virtual void draw_line_strip(const ff::point_float* points, size_t count, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_line(const ff::point_float& start, const ff::point_float& end, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4* colors) override;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color) override;
        virtual void draw_filled_triangles(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count) override;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color) override;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color) override;
        virtual void draw_outline_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_line_strip(const ff::point_float* points, const int* colors, size_t count, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_line_strip(const ff::point_float* points, size_t count, int color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_line(const ff::point_float& start, const ff::point_float& end, int color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, const int* colors) override;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, int color) override;
        virtual void draw_palette_filled_triangles(const ff::point_float* points, const int* colors, size_t count) override;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int color) override;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int inside_color, int outside_color) override;
        virtual void draw_palette_outline_rectangle(const ff::rect_float& rect, int color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int color, float thickness, bool pixel_thickness = false) override;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int inside_color, int outside_color, float thickness, bool pixel_thickness = false) override;

        virtual ff::matrix_stack& world_matrix_stack() override;
        virtual void nudge_depth() override;

        virtual void push_palette(ff::palette_base* palette) override;
        virtual void pop_palette() override;
        virtual void push_palette_remap(const uint8_t* remap, size_t hash) override;
        virtual void pop_palette_remap() override;
        virtual void push_no_overlap() override;
        virtual void pop_no_overlap() override;
        virtual void push_opaque() override;
        virtual void pop_opaque() override;
        virtual void push_pre_multiplied_alpha() override;
        virtual void pop_pre_multiplied_alpha() override;
        virtual void push_custom_context(ff::draw_base::custom_context_func&& func) override;
        virtual void pop_custom_context() override;
        virtual void push_texture_sampler(D3D11_FILTER filter) override;
        virtual void pop_texture_sampler() override;

    private:
        void destroy();
        bool init();

        void matrix_changing(const ff::matrix_stack& matrix_stack);
        void matrix_changed(const ff::matrix_stack& matrix_stack);

        void draw_line_strip(const ff::point_float* points, size_t point_count, const DirectX::XMFLOAT4* colors, size_t color_count, float thickness, bool pixel_thickness);

        void init_geometry_constant_buffers_0(ff::dx11_target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect);
        void update_geometry_constant_buffers_0();
        void update_geometry_constant_buffers_1();
        void update_pixel_constant_buffers_0();
        void update_palette_texture();
        void set_shader_input();

        void flush();
        bool create_geometry_buffer();
        void draw_opaque_geometry();
        void draw_alpha_geometry();
        void post_flush();

        bool is_rendering() const;
        float nudge_depth(::last_depth_type depth_type);
        unsigned int get_world_matrix_index();
        unsigned int get_world_matrix_index_no_flush();
        unsigned int get_texture_index_no_flush(const ff::dx11_texture_view_base& texture_view, bool use_palette);
        unsigned int get_palette_index_no_flush();
        unsigned int get_palette_remap_index_no_flush();
        int remap_palette_index(int color);
        void get_world_matrix_and_texture_index(const ff::dx11_texture_view_base& texture_view, bool use_palette, unsigned int& model_index, unsigned int& texture_index);
        void get_world_matrix_and_texture_indexes(ff::dx11_texture_view_base* const* texture_views, bool use_palette, unsigned int* texture_indexes, size_t count, unsigned int& model_index);
        void add_geometry(const void* data, ::geometry_bucket_type bucket_type, float depth);
        void* add_geometry(::geometry_bucket_type bucket_type, float depth);

        ff::dx11_fixed_state create_opaque_draw_state();
        ff::dx11_fixed_state create_alpha_draw_state();
        ff::dx11_fixed_state create_pre_multiplied_alpha_draw_state();
        ::geometry_bucket get_geometry_bucket(::geometry_bucket_type type);

        enum class state_t
        {
            invalid,
            valid,
            drawing,
        } state;

        // Constant data for shaders
        Microsoft::WRL::ComPtr<ID3D11Buffer> _geometryBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _geometryConstantsBuffer0;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _geometryConstantsBuffer1;
        Microsoft::WRL::ComPtr<ID3D11Buffer> _pixelConstantsBuffer0;
        ::geometry_shader_constants_0 _geometryConstants0;
        ::geometry_shader_constants_1 _geometryConstants1;
        ::pixel_shader_constants_0 _pixelConstants0;
        size_t _geometryConstantsHash0;
        size_t _geometryConstantsHash1;
        size_t _pixelConstantsHash0;

        // Render state
        std::vector<Microsoft::WRL::ComPtr<ID3D11SamplerState>> _samplerStack;
        ff::dx11_fixed_state _opaqueState;
        ff::dx11_fixed_state _alphaState;
        ff::dx11_fixed_state _premultipliedAlphaState;
        std::vector<ff::draw_base::custom_context_func> _customContextStack;

        // Matrixes
        DirectX::XMFLOAT4X4 _viewMatrix;
        ff::matrix_stack _worldMatrixStack;
        std::unordered_set<DirectX::XMFLOAT4X4, unsigned int, ff::stable_hash<DirectX::XMFLOAT4X4>> _worldMatrixToIndex;
        unsigned int _worldMatrixIndex;

        // Textures
        std::array<ff::dx11_texture_view_base*, ::MAX_TEXTURES> _textures;
        std::array<ff::dx11_texture_view_base*, ::MAX_TEXTURES_USING_PALETTE> _texturesUsingPalette;
        size_t _textureCount;
        size_t _texturesUsingPaletteCount;

        // Palettes
        bool _targetRequiresPalette;

        std::vector<ff::palette_base*> _paletteStack;
        std::shared_ptr<ff::dx11_texture> _paletteTexture;
        std::array<size_t, ::MAX_PALETTES> _paletteTextureHashes;
        std::unordered_map<size_t, std::pair<ff::palette_base*, unsigned int>, ff::no_hash<size_t>> _paletteToIndex;
        unsigned int _paletteIndex;

        std::vector<std::pair<const unsigned char*, size_t>> _paletteRemapStack;
        std::shared_ptr<ff::dx11_texture> _paletteRemapTexture;
        std::array<size_t, ::MAX_PALETTE_REMAPS> _paletteRemapTextureHashes;
        std::unordered_map<size_t, std::pair<const unsigned char*, unsigned int>, ff::no_hash<size_t>> _paletteRemapToIndex;
        unsigned int _paletteRemapIndex;

        // Render data
        std::vector<::alpha_geometry_entry> _alphaGeometry;
        std::array<::geometry_bucket, static_cast<size_t>(::geometry_bucket_type::count)> _geometryBuckets;
        ::last_depth_type _lastDepthType;
        float _drawDepth;
        int _forceNoOverlap;
        int _forceOpaque;
        int _forcePMA;
    };
}

std::unique_ptr<ff::draw_device> ff::draw_device::create()
{
    return nullptr; // TODO: std::make_unique<::draw_device_internal>();
}

ff::draw_ptr ff::draw_device::begin_draw(ff::dx11_target_base* target, ff::dx11_depth* depth, const ff::rect_fixed& view_rect, const ff::rect_fixed& world_rect, ff::draw_options options)
{
    return this->begin_draw(target, depth, std::floor(view_rect).cast<float>(), std::floor(world_rect).cast<float>(), options);
}
