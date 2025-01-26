#pragma once

#include "../dxgi/device_child_base.h"
#include "../dxgi/draw_base.h"
#include "../dxgi/palette_base.h"
#include "../dx_types/operators.h"
#include "../dx_types/matrix_stack.h"

namespace ff::dxgi
{
    class buffer_base;
    class command_context_base;
    class depth_base;
    class sprite_data;
    class target_base;
    class texture_base;
    class texture_view_base;

    enum class draw_options;
}

namespace ff::dxgi::draw_util
{
    namespace ffdu = ff::dxgi::draw_util;

    constexpr size_t MAX_TEXTURES = 32;
    constexpr size_t MAX_PALETTE_TEXTURES = 32;
    constexpr size_t MAX_PALETTES = 128; // 256 color palettes only
    constexpr size_t MAX_PALETTE_REMAPS = 128; // 256 entries only
    constexpr size_t MAX_TRANSFORM_MATRIXES = 128;
    constexpr size_t MAX_RENDER_COUNT = 524288; // 0x00080000 (not enforced)
    constexpr float MAX_RENDER_DEPTH = 1.0f;
    constexpr float RENDER_DEPTH_DELTA = MAX_RENDER_DEPTH / MAX_RENDER_COUNT;
    constexpr size_t MIN_INSTANCE_BUCKET_COUNT = 64;

    struct sprite_instance
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 uv_rect;
        DirectX::XMFLOAT4 color; // For palette out: R=1 to use sprite sample, <1 to override output (like for fonts)
        float depth;
        uint32_t matrix_texture_index; // matrix<<24, remap<<16, (palette or sampler)<<8, texture
    };

    struct rotated_sprite_instance
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 uv_rect;
        DirectX::XMFLOAT4 color; // For palette out: R=1 to use sprite sample, <1 to override output (like for fonts)
        DirectX::XMFLOAT4 pos_rot; // x,y, z=depth, w=rotate degrees CCW
        uint32_t indexes; // matrix<<24, remap<<16, (palette or sampler)<<8, texture
    };

    struct line_instance
    {
        DirectX::XMFLOAT2 start;
        DirectX::XMFLOAT2 end;
        DirectX::XMFLOAT4 start_color;
        DirectX::XMFLOAT4 end_color;
        float start_thickness;
        float end_thickness;
        float depth;
        uint32_t matrix_index;
    };

    struct line_strip_instance
    {
        DirectX::XMFLOAT2 start;
        DirectX::XMFLOAT2 end;
        DirectX::XMFLOAT2 before_start;
        DirectX::XMFLOAT2 after_end;
        DirectX::XMFLOAT4 start_color;
        DirectX::XMFLOAT4 end_color;
        float start_thickness;
        float end_thickness;
        float depth;
        uint32_t matrix_index;
    };

    struct triangle_filled_instance
    {
        DirectX::XMFLOAT2 position[3];
        DirectX::XMFLOAT4 color[3];
        float depth;
        uint32_t matrix_index;
    };

    struct rectangle_filled_instance
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 color;
        float depth;
        uint32_t matrix_index;
    };

    struct rectangle_outline_instance
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 color;
        float depth;
        float thickness;
        uint32_t matrix_index;
    };

    struct circle_filled_instance
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 inside_color;
        DirectX::XMFLOAT4 outside_color;
        float radius;
        uint32_t matrix_index;
    };

    struct circle_outline_instance
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 inside_color;
        DirectX::XMFLOAT4 outside_color;
        float radius;
        float thickness;
        uint32_t matrix_index;
    };

    enum class last_depth_type
    {
        none,
        instance,
        instance_no_overlap,
    };

    enum class instance_bucket_type
    {
        sprites,
        palette_sprites,
        rotated_sprites,
        rotated_palette_sprites,
        lines,
        line_strips,
        triangles,
        rectangles_filled,
        rectangles_outline,
        circles_filled,
        circles_outline,

        sprites_out_transparent,
        palette_sprites_out_transparent,
        rotated_sprites_out_transparent,
        rotated_palette_sprites_out_transparent,
        lines_out_transparent,
        line_strips_out_transparent,
        triangles_out_transparent,
        rectangles_filled_out_transparent,
        rectangles_outline_out_transparent,
        circles_filled_out_transparent,
        circles_outline_out_transparent,

        count,
        first_transparent = sprites_out_transparent,
    };

    class instance_bucket
    {
    private:
        instance_bucket(ffdu::instance_bucket_type bucket_type, const std::type_info& item_type, size_t item_size, size_t item_align);
        instance_bucket(ffdu::instance_bucket&& rhs) noexcept;

    public:
        ~instance_bucket();

        template<typename T, ffdu::instance_bucket_type BucketType>
        static ffdu::instance_bucket create()
        {
            return ffdu::instance_bucket(BucketType, typeid(T), sizeof(T), alignof(T));
        }

        void reset();
        void* add(const void* data = nullptr);
        size_t item_size() const;
        const std::type_info& item_type() const;
        ffdu::instance_bucket_type bucket_type() const;
        bool is_transparent() const;
        size_t count() const;
        void clear_items();
        size_t byte_size() const;
        const uint8_t* data() const;

        void render_start(size_t start);
        size_t render_start() const;
        size_t render_count() const;

    private:
        ffdu::instance_bucket_type bucket_type_;
        const std::type_info& item_type_;
        size_t item_size_{};
        size_t item_align{};
        size_t render_start_{};
        size_t render_count_{};
        uint8_t* data_start{};
        uint8_t* data_cur{};
        uint8_t* data_end{};
    };

    struct transparent_instance_entry
    {
        const ffdu::instance_bucket* bucket;
        size_t index;
        float depth;
    };

    struct vs_constants_0
    {
        static const size_t DWORD_COUNT = 18; // don't include padding

        DirectX::XMFLOAT4X4 projection;
        ff::point_float view_scale;
        float padding[2];
    };

    struct vs_constants_1
    {
        std::vector<DirectX::XMFLOAT4X4> model;
    };

    struct ps_constants_0
    {
        std::array<ff::rect_float, ffdu::MAX_PALETTE_TEXTURES> texture_palette_sizes;
    };

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
        virtual void draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::transform& transform) override;
        virtual void draw_lines(std::span<const ff::dxgi::endpoint_t> points) override;
        virtual void draw_triangles(std::span<const ff::dxgi::endpoint_t> points) override;
        virtual void draw_rectangle(const ff::rect_float& rect, const ff::color& color, std::optional<float> thickness) override;
        virtual void draw_circle(const ff::dxgi::endpoint_t& pos, std::optional<float> thickness, const ff::color* outside_color) override;

        virtual ff::matrix_stack& world_matrix_stack() override;
        virtual void push_palette(ff::dxgi::palette_base* palette) override;
        virtual void pop_palette() override;
        virtual void push_palette_remap(ff::dxgi::remap_t remap) override;
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
        using palette_to_index_t = std::unordered_map<size_t, std::pair<ff::dxgi::palette_base*, uint32_t>, ff::no_hash<size_t>>;
        using palette_remap_to_index_t = std::unordered_map<size_t, std::pair<ff::dxgi::remap_t, uint32_t>, ff::no_hash<size_t>>;

        virtual void internal_destroy() = 0;
        virtual void internal_reset() = 0;
        virtual ff::dxgi::command_context_base* internal_flush(ff::dxgi::command_context_base* context, bool end_draw) = 0;
        virtual ff::dxgi::command_context_base* internal_setup(ff::dxgi::command_context_base& context, ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, bool ignore_rotation) = 0;
        virtual void internal_flush_begin(ff::dxgi::command_context_base* context) = 0;
        virtual void internal_flush_end(ff::dxgi::command_context_base* context) = 0;

        virtual ff::dxgi::buffer_base& instance_buffer() = 0;
        virtual ff::dxgi::buffer_base& vs_constants_buffer_0() = 0;
        virtual ff::dxgi::buffer_base& vs_constants_buffer_1() = 0;
        virtual ff::dxgi::buffer_base& ps_constants_buffer_0() = 0;
        virtual bool flush_for_sampler_change() const = 0;

        virtual void update_palette_texture(ff::dxgi::command_context_base& context,
            size_t textures_using_palette_count,
            ff::dxgi::texture_base& palette_texture, size_t* palette_texture_hashes, palette_to_index_t& palette_to_index,
            ff::dxgi::texture_base& palette_remap_texture, size_t* palette_remap_texture_hashes, palette_remap_to_index_t& palette_remap_to_index) = 0;
        virtual void apply_shader_input(ff::dxgi::command_context_base& context,
            size_t texture_count, ff::dxgi::texture_view_base** textures,
            size_t textures_using_palette_count, ff::dxgi::texture_view_base** textures_using_palette,
            ff::dxgi::texture_base& palette_texture, ff::dxgi::texture_base& palette_remap_texture) = 0;
        virtual void apply_opaque_state(ff::dxgi::command_context_base& context) = 0;
        virtual void apply_transparent_state(ff::dxgi::command_context_base& context) = 0;
        virtual bool apply_instance_state(ff::dxgi::command_context_base& context, const ffdu::instance_bucket& bucket) = 0;
        virtual std::shared_ptr<ff::dxgi::texture_base> create_texture(ff::point_size size, DXGI_FORMAT format) = 0;
        virtual void draw(ff::dxgi::command_context_base& context, ffdu::instance_bucket_type instance_type, size_t instance_start, size_t instance_count) = 0;

        ff::dxgi::device_child_base* as_device_child();
        bool internal_valid() const;
        bool linear_sampler() const;
        bool target_requires_palette() const;
        bool pre_multiplied_alpha() const;

        ff::dxgi::draw_ptr internal_begin_draw(
            ff::dxgi::command_context_base& context,
            ff::dxgi::target_base& target,
            ff::dxgi::depth_base* depth,
            const ff::rect_float& view_rect,
            const ff::rect_float& world_rect,
            ff::dxgi::draw_options options);

    private:
        // device_child_base
        virtual bool reset() override;

        void destroy();
        void flush(bool end_draw = false);

        void matrix_changing(const ff::matrix_stack& matrix_stack);
        void init_vs_constants_buffer_0(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect);
        void update_vs_constants_buffer_0();
        void update_vs_constants_buffer_1();
        void update_ps_constants_buffer_0();
        bool create_instance_buffer();
        void draw_opaque_instances();
        void draw_transparent_instances();
        float nudge_depth(ffdu::last_depth_type depth_type);

        uint32_t get_world_matrix_index();
        uint32_t get_world_matrix_index_no_flush();
        uint32_t get_texture_index_no_flush(ff::dxgi::texture_view_base& texture_view, bool use_palette);
        uint32_t get_palette_index_no_flush();
        uint32_t get_palette_remap_index_no_flush();
        int remap_palette_index(int color) const;
        uint32_t get_world_matrix_and_texture_index(ff::dxgi::texture_view_base& texture_view, bool use_palette);

        void* add_instance(const void* data, ffdu::instance_bucket_type bucket_type, float depth);
        ffdu::instance_bucket& get_instance_bucket(ffdu::instance_bucket_type type);

        // State
        enum class state_t
        {
            invalid,
            valid,
            drawing,
        } state{};

        ff::dxgi::command_context_base* command_context_{};

        // Constant data for shaders
        ffdu::vs_constants_0 vs_constants_0{};
        ffdu::vs_constants_1 vs_constants_1{};
        ffdu::ps_constants_0 ps_constants_0{};

        // Render state
        std::vector<bool> sampler_stack;
        std::vector<ff::dxgi::draw_base::custom_context_func> custom_context_stack;

        // Matrixes
        DirectX::XMFLOAT4X4 view_matrix{};
        ff::matrix_stack world_matrix_stack_;
        ff::signal_connection world_matrix_stack_changing_connection;
        std::unordered_map<DirectX::XMFLOAT4X4, uint32_t, ff::stable_hash<DirectX::XMFLOAT4X4>> world_matrix_to_index;
        uint32_t world_matrix_index{};

        // Textures
        std::array<ff::dxgi::texture_view_base*, ffdu::MAX_TEXTURES> textures{};
        std::array<ff::dxgi::texture_view_base*, ffdu::MAX_PALETTE_TEXTURES> textures_using_palette{};
        size_t texture_count{};
        size_t textures_using_palette_count{};

        // Palettes
        bool target_requires_palette_{};

        std::vector<ff::dxgi::palette_base*> palette_stack;
        std::shared_ptr<ff::dxgi::texture_base> palette_texture;
        std::array<size_t, ffdu::MAX_PALETTES> palette_texture_hashes{};
        palette_to_index_t palette_to_index;
        uint32_t palette_index{};

        std::vector<ff::dxgi::remap_t> palette_remap_stack;
        std::shared_ptr<ff::dxgi::texture_base> palette_remap_texture;
        std::array<size_t, ffdu::MAX_PALETTE_REMAPS> palette_remap_texture_hashes{};
        palette_remap_to_index_t palette_remap_to_index;
        uint32_t palette_remap_index{};

        // Render data
        std::vector<ffdu::transparent_instance_entry> transparent_instances;
        std::array<ffdu::instance_bucket, static_cast<size_t>(ffdu::instance_bucket_type::count)> instance_buckets;
        ffdu::last_depth_type last_depth_type{};
        float draw_depth{};
        int force_no_overlap{};
        int force_opaque{};
        int force_pre_multiplied_alpha{};
    };
}
