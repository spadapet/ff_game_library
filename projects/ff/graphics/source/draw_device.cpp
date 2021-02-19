#include "pch.h"
#include "draw_device.h"
#include "graphics.h"
#include "graphics_child_base.h"
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

#if 0
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
            : bucket_type(bucket_type)
            , item_type(&item_type)
            , item_size(item_size)
            , item_align(item_align)
            , render_start(0)
            , render_count(0)
            , data_start(nullptr)
            , data_cur(nullptr)
            , data_end(nullptr)
            , element_desc(element_desc)
            , element_count(element_count)
        {}

    public:
        template<typename T, geometry_bucket_type BucketType>
        static geometry_bucket New()
        {
            return geometry_bucket(BucketType, typeid(T), sizeof(T), alignof(T), T::GetLayout11().data(), T::GetLayout11().size());
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
            , bucket_type(rhs.bucket_type)
            , item_type(rhs.item_type)
            , item_size(rhs.item_size)
            , item_align(rhs.item_align)
            , render_start(0)
            , render_count(0)
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
            _aligned_free(this->data_start);
        }

        // Input strings must be static
        void Reset(const wchar_t* vsRes, const wchar_t* gsRes, const wchar_t* psRes, const wchar_t* psPaletteOutRes)
        {
            Reset();

            ff::IResourceAccess* resources = ff::GetThisModule().GetResources();
            this->vs_res.Init(resources, ff::String::from_static(vsRes));
            this->gs_res.Init(resources, ff::String::from_static(gsRes));
            this->ps_res.Init(resources, ff::String::from_static(psRes));
            this->ps_palette_out_res.Init(resources, ff::String::from_static(psPaletteOutRes));
        }

        void Reset()
        {
            this->layout = nullptr;
            this->vs = nullptr;
            this->gs = nullptr;
            this->ps = nullptr;
            this->ps_palette_out = nullptr;

            _aligned_free(this->data_start);
            this->data_start = nullptr;
            this->data_cur = nullptr;
            this->data_end = nullptr;
        }

        void Apply(ff::GraphContext11& context, ff::GraphStateCache11& cache, ID3D11Buffer* geometryBuffer, bool paletteOut) const
        {
            const_cast<geometry_bucket*>(this)->CreateShaders(cache, paletteOut);

            context.SetVertexIA(geometryBuffer, GetItemByteSize(), 0);
            context.SetLayoutIA(this->layout);
            context.SetVS(this->vs);
            context.SetGS(this->gs);
            context.SetPS(paletteOut ? this->ps_palette_out : this->ps);
        }

        void* Add(const void* data = nullptr)
        {
            if (this->data_cur == this->data_end)
            {
                size_t curSize = this->data_end - this->data_start;
                size_t newSize = std::max<size_t>(curSize * 2, this->item_size * 64);
                this->data_start = (uint8_t*)_aligned_realloc(this->data_start, newSize, this->item_align);
                this->data_cur = this->data_start + curSize;
                this->data_end = this->data_start + newSize;
            }

            if (data)
            {
                std::memcpy(this->data_cur, data, this->item_size);
            }

            void* result = this->data_cur;
            this->data_cur += this->item_size;
            return result;
        }

        size_t GetItemByteSize() const
        {
            return this->item_size;
        }

        const std::type_info& GetItemType() const
        {
            return *this->item_type;
        }

        geometry_bucket_type GetBucketType() const
        {
            return this->bucket_type;
        }

        size_t GetCount() const
        {
            return (this->data_cur - this->data_start) / this->item_size;
        }

        void ClearItems()
        {
            this->data_cur = this->data_start;
        }

        size_t GetByteSize() const
        {
            return this->data_cur - this->data_start;
        }

        const void* GetData() const
        {
            return this->data_start;
        }

        void SetRenderStart(size_t renderStart)
        {
            this->render_start = renderStart;
            this->render_count = GetCount();
        }

        size_t GetRenderStart() const
        {
            return this->render_start;
        }

        size_t GetRenderCount() const
        {
            return this->render_count;
        }

    private:
        void CreateShaders(ff::GraphStateCache11& cache, bool paletteOut)
        {
            if (!this->vs)
            {
                this->vs = cache.GetVertexShaderAndInputLayout(ff::GetThisModule().GetResources(), this->vs_res.GetName(), this->layout, this->element_desc, this->element_count);
            }

            if (!this->gs)
            {
                this->gs = cache.GetGeometryShader(ff::GetThisModule().GetResources(), this->gs_res.GetName());
            }

            ff::ComPtr<ID3D11PixelShader>& ps = paletteOut ? this->ps_palette_out : this->ps;
            if (!ps)
            {
                ps = cache.GetPixelShader(ff::GetThisModule().GetResources(), paletteOut ? this->ps_palette_out_res.GetName() : this->ps_res.GetName());
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

        geometry_bucket_type bucket_type;
        const std::type_info* item_type;
        size_t item_size;
        size_t item_align;
        size_t render_start;
        size_t render_count;
        uint8_t* data_start;
        uint8_t* data_cur;
        uint8_t* data_end;
    };
#endif

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
        enum class state_t
        {
            invalid,
            valid,
            drawing,
        } state;
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
