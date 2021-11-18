#pragma once

#include "device_child_base.h"
#include "draw_base.h"
#include "draw_ptr.h"
#include "matrix_stack.h"
#include "palette_base.h"

namespace ff::dxgi
{
    class buffer_base;
    class command_context_base;
    class depth_base;
    class sprite_data;
    class target_base;
    class texture_base;
    class texture_view_base;
}

namespace ff::dxgi::draw_util
{
    constexpr size_t MAX_TEXTURES = 32;
    constexpr size_t MAX_TEXTURES_USING_PALETTE = 32;
    constexpr size_t MAX_PALETTES = 128; // 256 color palettes only
    constexpr size_t MAX_PALETTE_REMAPS = 128; // 256 entries only
    constexpr size_t MAX_TRANSFORM_MATRIXES = 1024;
    constexpr size_t MAX_RENDER_COUNT = 524288; // 0x00080000
    constexpr float MAX_RENDER_DEPTH = 1.0f;
    constexpr float RENDER_DEPTH_DELTA = MAX_RENDER_DEPTH / MAX_RENDER_COUNT;

    const std::array<uint8_t, ff::dxgi::palette_size>& default_palette_remap();
    size_t default_palette_remap_hash();

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
        geometry_bucket(ff::dxgi::draw_util::geometry_bucket_type bucket_type, const std::type_info& item_type, size_t item_size, size_t item_align);
        geometry_bucket(ff::dxgi::draw_util::geometry_bucket&& rhs) noexcept;

    public:
        virtual ~geometry_bucket();

        template<typename T, ff::dxgi::draw_util::geometry_bucket_type BucketType>
        static ff::dxgi::draw_util::geometry_bucket create()
        {
            return ff::dxgi::draw_util::geometry_bucket(BucketType, typeid(T), sizeof(T), alignof(T));
        }

        void reset();
        void* add(const void* data = nullptr);
        size_t item_size() const;
        const std::type_info& item_type() const;
        ff::dxgi::draw_util::geometry_bucket_type bucket_type() const;
        size_t count() const;
        void clear_items();
        size_t byte_size() const;
        const uint8_t* data() const;

        void render_start(size_t start);
        size_t render_start() const;
        size_t render_count() const;

    private:
        ff::dxgi::draw_util::geometry_bucket_type bucket_type_;
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
        const ff::dxgi::draw_util::geometry_bucket* bucket;
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
        std::array<ff::rect_float, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE> texture_palette_sizes;
    };

    ff::dxgi::draw_util::alpha_type get_alpha_type(const DirectX::XMFLOAT4& color, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const ff::dxgi::sprite_data& data, const DirectX::XMFLOAT4& color, bool force_opaque);
    ff::dxgi::draw_util::alpha_type get_alpha_type(const ff::dxgi::sprite_data** datas, const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque);

    ff::rect_float get_rotated_view_rect(ff::dxgi::target_base& target, const ff::rect_float& view_rect);
    DirectX::XMMATRIX get_view_matrix(const ff::rect_float& world_rect);
    DirectX::XMMATRIX get_orientation_matrix(ff::dxgi::target_base& target, const ff::rect_float& view_rect, ff::point_float world_center);
    bool setup_view_matrix(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect, DirectX::XMFLOAT4X4& view_matrix);

    class draw_device_base : public ff::dxgi::draw_base, private ff::dxgi::device_child_base
    {
    public:
        draw_device_base();
        virtual ~draw_device_base() override;

        draw_device_base(draw_device_base&& other) noexcept = delete;
        draw_device_base(const draw_device_base& other) = delete;
        draw_device_base& operator=(draw_device_base&& other) noexcept = delete;
        draw_device_base& operator=(const draw_device_base& other) = delete;

        virtual void end_draw() override;
        virtual void draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::dxgi::transform& transform) override;
        virtual void draw_line_strip(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count, float thickness, bool pixel_thickness) override;
        virtual void draw_line_strip(const ff::point_float* points, size_t count, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override;
        virtual void draw_line(const ff::point_float& start, const ff::point_float& end, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4* colors) override;
        virtual void draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color) override;
        virtual void draw_filled_triangles(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count) override;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color) override;
        virtual void draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color) override;
        virtual void draw_outline_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness) override;
        virtual void draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color, float thickness, bool pixel_thickness) override;

        virtual void draw_palette_line_strip(const ff::point_float* points, const int* colors, size_t count, float thickness, bool pixel_thickness) override;
        virtual void draw_palette_line_strip(const ff::point_float* points, size_t count, int color, float thickness, bool pixel_thickness) override;
        virtual void draw_palette_line(const ff::point_float& start, const ff::point_float& end, int color, float thickness, bool pixel_thickness) override;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, const int* colors) override;
        virtual void draw_palette_filled_rectangle(const ff::rect_float& rect, int color) override;
        virtual void draw_palette_filled_triangles(const ff::point_float* points, const int* colors, size_t count) override;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int color) override;
        virtual void draw_palette_filled_circle(const ff::point_float& center, float radius, int inside_color, int outside_color) override;
        virtual void draw_palette_outline_rectangle(const ff::rect_float& rect, int color, float thickness, bool pixel_thickness) override;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int color, float thickness, bool pixel_thickness) override;
        virtual void draw_palette_outline_circle(const ff::point_float& center, float radius, int inside_color, int outside_color, float thickness, bool pixel_thickness) override;

        virtual ff::dxgi::matrix_stack& world_matrix_stack() override;
        virtual void nudge_depth() override;
        virtual void push_palette(ff::dxgi::palette_base* palette) override;
        virtual void pop_palette() override;
        virtual void push_palette_remap(const uint8_t* remap, size_t hash) override;
        virtual void pop_palette_remap() override;
        virtual void push_no_overlap() override;
        virtual void pop_no_overlap() override;
        virtual void push_opaque() override;
        virtual void pop_opaque() override;
        virtual void push_pre_multiplied_alpha() override;
        virtual void pop_pre_multiplied_alpha() override;
        virtual void push_custom_context(ff::dxgi::draw_base::custom_context_func&& func) override;
        virtual void pop_custom_context() override;
        virtual void push_sampler_linear_filter(bool linear_filter) override;
        virtual void pop_sampler_linear_filter() override;

    protected:
        using palette_to_index_t = std::unordered_map<size_t, std::pair<ff::dxgi::palette_base*, unsigned int>, ff::no_hash<size_t>>;
        using palette_remap_to_index_t = std::unordered_map<size_t, std::pair<const uint8_t*, unsigned int>, ff::no_hash<size_t>>;

        virtual void internal_destroy() = 0;
        virtual void internal_reset() = 0;
        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base& context, bool end_draw) = 0;
        virtual ff::dxgi::command_context_base* internal_setup(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect) = 0;

        virtual ff::dxgi::buffer_base& geometry_buffer() = 0;
        virtual ff::dxgi::buffer_base& geometry_constants_buffer_0() = 0;
        virtual ff::dxgi::buffer_base& geometry_constants_buffer_1() = 0;
        virtual ff::dxgi::buffer_base& pixel_constants_buffer_0() = 0;

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            bool target_requires_palette, size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) = 0;
        virtual void apply_shader_input(ff::dxgi::command_context_base& context,
            bool target_requires_palette, bool linear_sampler,
            size_t texture_count, const ff::dxgi::texture_view_base** textures,
            size_t textures_using_palette_count, const ff::dxgi::texture_view_base** textures_using_palette,
            ff::dxgi::texture_base& palette_texture, ff::dxgi::texture_base& palette_remap_texture) = 0;
        virtual void apply_opaque_state(ff::dxgi::command_context_base& context) = 0;
        virtual void apply_alpha_state(ff::dxgi::command_context_base& context, bool force_pre_multiplied_alpha) = 0;
        virtual void apply_geometry_buffer(ff::dxgi::command_context_base& context, ff::dxgi::draw_util::geometry_bucket_type bucket_type, ff::dxgi::buffer_base& geometry_buffer, bool target_requires_palette) = 0;
        virtual std::shared_ptr<ff::dxgi::texture_base> create_texture(ff::point_size size, DXGI_FORMAT format) = 0;
        virtual void draw(ff::dxgi::command_context_base& context, size_t count, size_t start) = 0;

        ff::dxgi::device_child_base* as_device_child();
        bool internal_valid() const;
        ff::dxgi::draw_ptr internal_begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::dxgi::draw_options options);

    private:
        // device_child_base
        virtual bool reset() override;

        void destroy();
        void flush(bool end_draw = false);
        ff::dxgi::command_context_base* setup(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect);

        void matrix_changing(const ff::dxgi::matrix_stack& matrix_stack);
        void draw_line_strip(const ff::point_float* points, size_t point_count, const DirectX::XMFLOAT4* colors, size_t color_count, float thickness, bool pixel_thickness);
        void init_geometry_constant_buffers_0(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect);
        void update_geometry_constant_buffers_0();
        void update_geometry_constant_buffers_1();
        void update_pixel_constant_buffers_0();
        bool create_geometry_buffer();
        void draw_opaque_geometry();
        void draw_alpha_geometry();
        float nudge_depth(ff::dxgi::draw_util::last_depth_type depth_type);

        unsigned int get_world_matrix_index();
        unsigned int get_world_matrix_index_no_flush();
        unsigned int get_texture_index_no_flush(const ff::dxgi::texture_view_base& texture_view, bool use_palette);
        unsigned int get_palette_index_no_flush();
        unsigned int get_palette_remap_index_no_flush();
        int remap_palette_index(int color) const;
        void get_world_matrix_and_texture_index(const ff::dxgi::texture_view_base& texture_view, bool use_palette, unsigned int& model_index, unsigned int& texture_index);
        void get_world_matrix_and_texture_indexes(ff::dxgi::texture_view_base* const* texture_views, bool use_palette, unsigned int* texture_indexes, size_t count, unsigned int& model_index);

        void* add_geometry(const void* data, ff::dxgi::draw_util::geometry_bucket_type bucket_type, float depth);
        ff::dxgi::draw_util::geometry_bucket& get_geometry_bucket(ff::dxgi::draw_util::geometry_bucket_type type);

        // State
        enum class state_t
        {
            invalid,
            valid,
            drawing,
        } state;

        ff::dxgi::command_context_base* command_context_;

        // Constant data for shaders
        ff::dxgi::draw_util::geometry_shader_constants_0 geometry_constants_0;
        ff::dxgi::draw_util::geometry_shader_constants_1 geometry_constants_1;
        ff::dxgi::draw_util::pixel_shader_constants_0 pixel_constants_0;
        size_t geometry_constants_hash_0;
        size_t geometry_constants_hash_1;
        size_t pixel_constants_hash_0;

        // Render state
        std::vector<bool> sampler_stack;
        std::vector<ff::dxgi::draw_base::custom_context_func> custom_context_stack;

        // Matrixes
        DirectX::XMFLOAT4X4 view_matrix;
        ff::dxgi::matrix_stack world_matrix_stack_;
        ff::signal_connection world_matrix_stack_changing_connection;
        std::unordered_map<DirectX::XMFLOAT4X4, unsigned int, ff::stable_hash<DirectX::XMFLOAT4X4>> world_matrix_to_index;
        unsigned int world_matrix_index;

        // Textures
        std::array<const ff::dxgi::texture_view_base*, ff::dxgi::draw_util::MAX_TEXTURES> textures;
        std::array<const ff::dxgi::texture_view_base*, ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE> textures_using_palette;
        size_t texture_count;
        size_t textures_using_palette_count;

        // Palettes
        bool target_requires_palette;

        std::vector<ff::dxgi::palette_base*> palette_stack;
        std::shared_ptr<ff::dxgi::texture_base> palette_texture;
        std::array<size_t, ff::dxgi::draw_util::MAX_PALETTES> palette_texture_hashes;
        palette_to_index_t palette_to_index;
        unsigned int palette_index;

        std::vector<std::pair<const uint8_t*, size_t>> palette_remap_stack;
        std::shared_ptr<ff::dxgi::texture_base> palette_remap_texture;
        std::array<size_t, ff::dxgi::draw_util::MAX_PALETTE_REMAPS> palette_remap_texture_hashes;
        palette_remap_to_index_t palette_remap_to_index;
        unsigned int palette_remap_index;

        // Render data
        std::vector<ff::dxgi::draw_util::alpha_geometry_entry> alpha_geometry;
        std::array<ff::dxgi::draw_util::geometry_bucket, static_cast<size_t>(ff::dxgi::draw_util::geometry_bucket_type::count)> geometry_buckets;
        ff::dxgi::draw_util::last_depth_type last_depth_type;
        float draw_depth;
        int force_no_overlap;
        int force_opaque;
        int force_pre_multiplied_alpha;
    };
}
