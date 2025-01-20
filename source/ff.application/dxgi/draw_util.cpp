#include "pch.h"
#include "dxgi/buffer_base.h"
#include "dxgi/depth_base.h"
#include "dxgi/draw_device_base.h"
#include "dxgi/draw_util.h"
#include "dxgi/format_util.h"
#include "dxgi/palette_data_base.h"
#include "dxgi/sprite_data.h"
#include "dxgi/target_base.h"
#include "dxgi/texture_base.h"
#include "dxgi/texture_view_base.h"
#include "dx_types/matrix.h"
#include "dx_types/operators.h"
#include "dx_types/transform.h"

namespace ffdu = ff::dxgi::draw_util;

constexpr uint32_t INVALID_INDEX = ff::constants::invalid_unsigned<uint32_t>();

namespace
{
    enum class alpha_type
    {
        opaque,
        transparent,
        invisible,
    };
}

static ::alpha_type get_alpha_type(const DirectX::XMFLOAT4& color, bool force_opaque)
{
    if (color.w == 0)
    {
        return ::alpha_type::invisible;
    }

    if (color.w == 1 || force_opaque)
    {
        return ::alpha_type::opaque;
    }

    return ::alpha_type::transparent;
}

static ::alpha_type get_alpha_type(const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    ::alpha_type type = ::get_alpha_type(colors[0], force_opaque);

    for (size_t i = 1; i < count; i++)
    {
        if (type != ::get_alpha_type(colors[i], force_opaque))
        {
            return ::alpha_type::transparent;
        }
    }

    return type;
}

static ::alpha_type get_alpha_type(const ff::dxgi::sprite_data& data, const DirectX::XMFLOAT4& color, bool force_opaque)
{
    switch (::get_alpha_type(color, force_opaque))
    {
        case ::alpha_type::transparent:
            return ff::flags::has(data.type(), ff::dxgi::sprite_type::palette) ? alpha_type::opaque : alpha_type::transparent;

        case ::alpha_type::opaque:
            return (ff::flags::has(data.type(), ff::dxgi::sprite_type::transparent) && !force_opaque)
                ? ::alpha_type::transparent
                : ::alpha_type::opaque;

        default:
            return ::alpha_type::invisible;
    }
}

static ::alpha_type get_alpha_type(const ff::dxgi::sprite_data** datas, const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    ::alpha_type type = ::get_alpha_type(*datas[0], colors[0], force_opaque);

    for (size_t i = 1; i < count; i++)
    {
        if (type != ::get_alpha_type(*datas[i], colors[i], force_opaque))
        {
            return ::alpha_type::transparent;
        }
    }

    return type;
}

static ff::dxgi::remap_t default_palette_remap()
{
    static std::array<uint8_t, ff::dxgi::palette_size> value
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

    static size_t hash = ff::stable_hash_bytes(value.data(), ff::array_byte_size(value));

    return { value, hash };
}

ffdu::instance_bucket::instance_bucket(ffdu::instance_bucket_type bucket_type, const std::type_info& item_type, size_t item_size, size_t item_align)
    : bucket_type_(bucket_type)
    , item_type_(item_type)
    , item_size_(item_size)
    , item_align(item_align)
{
}

ffdu::instance_bucket::instance_bucket(ffdu::instance_bucket&& rhs) noexcept
    : bucket_type_(rhs.bucket_type_)
    , item_type_(rhs.item_type_)
    , item_size_(rhs.item_size_)
    , item_align(rhs.item_align)
    , data_start(rhs.data_start)
    , data_cur(rhs.data_cur)
    , data_end(rhs.data_end)
{
    rhs.data_start = nullptr;
    rhs.data_cur = nullptr;
    rhs.data_end = nullptr;
}

ffdu::instance_bucket::~instance_bucket()
{
    ::_aligned_free(this->data_start);
}

void ffdu::instance_bucket::reset()
{
    ::_aligned_free(this->data_start);
    this->data_start = nullptr;
    this->data_cur = nullptr;
    this->data_end = nullptr;
}

void* ffdu::instance_bucket::add(const void* data)
{
    if (this->data_cur == this->data_end)
    {
        size_t cur_size = this->data_end - this->data_start;
        size_t new_size = std::max<size_t>(cur_size * 2, this->item_size_ * ffdu::MIN_INSTANCE_BUCKET_COUNT);
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

size_t ffdu::instance_bucket::item_size() const
{
    return this->item_size_;
}

const std::type_info& ffdu::instance_bucket::item_type() const
{
    return this->item_type_;
}

ffdu::instance_bucket_type ffdu::instance_bucket::bucket_type() const
{
    return this->bucket_type_;
}

size_t ffdu::instance_bucket::count() const
{
    return (this->data_cur - this->data_start) / this->item_size_;
}

void ffdu::instance_bucket::clear_items()
{
    this->data_cur = this->data_start;
}

size_t ffdu::instance_bucket::byte_size() const
{
    return this->data_cur - this->data_start;
}

const uint8_t* ffdu::instance_bucket::data() const
{
    return this->data_start;
}

void ffdu::instance_bucket::render_start(size_t start)
{
    this->render_start_ = start;
    this->render_count_ = this->count();
}

size_t ffdu::instance_bucket::render_start() const
{
    return this->render_start_;
}

size_t ffdu::instance_bucket::render_count() const
{
    return this->render_count_;
}

static const DirectX::XMMATRIX& get_rotate_matrix(int dmod, bool ignore_rotate)
{
    static const DirectX::XMFLOAT4X4A rotate_0(
        2, 0, 0, 0,
        0, -2, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1);

    static const DirectX::XMFLOAT4X4A rotate_90(
        0, 2, 0, 0,
        2, 0, 0, 0,
        0, 0, 1, 0,
        -1, -1, 0, 1);

    static const DirectX::XMFLOAT4X4A ignore_rotate_90(
        2, 0, 0, 0,
        0, -2, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1);

    static const DirectX::XMFLOAT4X4A rotate_180(
        -2, 0, 0, 0,
        0, 2, 0, 0,
        0, 0, 1, 0,
        1, -1, 0, 1);

    static const DirectX::XMFLOAT4X4A ignore_rotate_180(
        2, 0, 0, 0,
        0, -2, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1);

    static const DirectX::XMFLOAT4X4A rotate_270(
        0, -2, 0, 0,
        -2, 0, 0, 0,
        0, 0, 1, 0,
        1, 1, 0, 1);

    static const DirectX::XMFLOAT4X4A ignore_rotate_270(
        2, 0, 0, 0,
        0, -2, 0, 0,
        0, 0, 1, 0,
        -1, 1, 0, 1);

    static const DirectX::XMMATRIX matrices[4][2] =
    {
        { DirectX::XMLoadFloat4x4(&rotate_0), DirectX::XMLoadFloat4x4(&rotate_0) },
        { DirectX::XMLoadFloat4x4(&rotate_90), DirectX::XMLoadFloat4x4(&ignore_rotate_90) },
        { DirectX::XMLoadFloat4x4(&rotate_180), DirectX::XMLoadFloat4x4(&ignore_rotate_180) },
        { DirectX::XMLoadFloat4x4(&rotate_270), DirectX::XMLoadFloat4x4(&ignore_rotate_270) },
    };

    return matrices[dmod][ignore_rotate ? 1 : 0];
}

static DirectX::XMMATRIX get_view_matrix(const ff::window_size& target_size, const ff::rect_float& world_rect, bool ignore_rotation)
{
    DirectX::XMMATRIX rotate_matrix = ::get_rotate_matrix(target_size.rotation, ignore_rotation);
    DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScaling(1 / world_rect.width(), 1 / world_rect.height(), 1);
    DirectX::XMMATRIX translate_matrix = DirectX::XMMatrixTranslation(-world_rect.left, -world_rect.top, 0);

    return translate_matrix * scale_matrix * rotate_matrix;
}

static bool setup_view_matrix(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect, DirectX::XMFLOAT4X4& view_matrix, bool ignore_rotation)
{
    check_ret_val(world_rect.area() != 0 && view_rect.area() > 0, false);

    DirectX::XMMATRIX view_matrix2 = ::get_view_matrix(target.size(), world_rect, ignore_rotation);
    DirectX::XMStoreFloat4x4(&view_matrix, DirectX::XMMatrixTranspose(view_matrix2));
    return true;
}

ffdu::draw_device_base::draw_device_base()
    : world_matrix_stack_changing_connection(this->world_matrix_stack_.matrix_changing().connect(std::bind(&draw_device_base::matrix_changing, this, std::placeholders::_1)))
    , instance_buckets
    {
        ffdu::instance_bucket::create<ffdu::sprite_instance, ffdu::instance_bucket_type::sprites>(),
        ffdu::instance_bucket::create<ffdu::sprite_instance, ffdu::instance_bucket_type::palette_sprites>(),
        ffdu::instance_bucket::create<ffdu::rotated_sprite_instance, ffdu::instance_bucket_type::rotated_sprites>(),
        ffdu::instance_bucket::create<ffdu::rotated_sprite_instance, ffdu::instance_bucket_type::rotated_palette_sprites>(),
        ffdu::instance_bucket::create<ffdu::line_instance, ffdu::instance_bucket_type::lines>(),
        ffdu::instance_bucket::create<ffdu::line_strip_instance, ffdu::instance_bucket_type::line_strips>(),
        ffdu::instance_bucket::create<ffdu::triangle_filled_instance, ffdu::instance_bucket_type::triangles>(),
        ffdu::instance_bucket::create<ffdu::rectangle_filled_instance, ffdu::instance_bucket_type::rectangles_filled>(),
        ffdu::instance_bucket::create<ffdu::rectangle_outline_instance, ffdu::instance_bucket_type::rectangles_outline>(),
        ffdu::instance_bucket::create<ffdu::circle_filled_instance, ffdu::instance_bucket_type::circles_filled>(),
        ffdu::instance_bucket::create<ffdu::circle_outline_instance, ffdu::instance_bucket_type::circles_outline>(),

        ffdu::instance_bucket::create<ffdu::sprite_instance, ffdu::instance_bucket_type::sprites_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::sprite_instance, ffdu::instance_bucket_type::palette_sprites_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::rotated_sprite_instance, ffdu::instance_bucket_type::rotated_sprites_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::rotated_sprite_instance, ffdu::instance_bucket_type::rotated_palette_sprites_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::line_instance, ffdu::instance_bucket_type::lines_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::line_strip_instance, ffdu::instance_bucket_type::line_strips_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::triangle_filled_instance, ffdu::instance_bucket_type::triangles_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::rectangle_filled_instance, ffdu::instance_bucket_type::rectangles_filled_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::rectangle_outline_instance, ffdu::instance_bucket_type::rectangles_outline_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::circle_filled_instance, ffdu::instance_bucket_type::circles_filled_out_transparent>(),
        ffdu::instance_bucket::create<ffdu::circle_outline_instance, ffdu::instance_bucket_type::circles_outline_out_transparent>(),
    }
{
}

ffdu::draw_device_base::~draw_device_base()
{
    assert(this->state != draw_device_base::state_t::drawing);
}

void ffdu::draw_device_base::end_draw()
{
    check_ret(this->state == state_t::drawing);

    this->flush(true);

    this->state = draw_device_base::state_t::valid;
    this->command_context_ = nullptr;
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

void ffdu::draw_device_base::draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::transform& transform)
{
#if 0
    ::alpha_type alpha_type = ::get_alpha_type(sprite, transform.color, this->force_opaque || this->target_requires_palette_);
    if (alpha_type != ::alpha_type::invisible && sprite.view())
    {
        bool use_palette = ff::flags::has(sprite.type(), ff::dxgi::sprite_type::palette);
        ffdu::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent && !this->target_requires_palette_)
            ? (use_palette ? ffdu::geometry_bucket_type::palette_sprites : ffdu::geometry_bucket_type::sprites_alpha)
            : (use_palette ? ffdu::geometry_bucket_type::palette_sprites : ffdu::geometry_bucket_type::sprites);

        float depth = this->nudge_depth(this->force_no_overlap ? ffdu::last_depth_type::instance_no_overlap : ffdu::last_depth_type::instance);
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
#endif
}

void ffdu::draw_device_base::draw_lines(std::span<const ff::dxgi::endpoint_t> points)
{
#if 0
    assert(color_count == 1 || color_count == point_count);
    thickness = pixel_thickness ? -std::abs(thickness) : std::abs(thickness);

    ff::vertex::line_geometry input;
    input.matrix_index = (this->world_matrix_index == ::INVALID_INDEX) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.depth = this->nudge_depth(this->force_no_overlap ? ffdu::last_depth_type::instance_no_overlap : ffdu::last_depth_type::instance);
    input.color[0] = colors[0];
    input.color[1] = colors[0];
    input.thickness[0] = thickness;
    input.thickness[1] = thickness;

    const DirectX::XMFLOAT2* dxpoints = reinterpret_cast<const DirectX::XMFLOAT2*>(points);
    bool closed = point_count > 2 && points[0] == points[point_count - 1];
    ::alpha_type alpha_type = ::get_alpha_type(colors[0], this->force_opaque || this->target_requires_palette_);

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
            alpha_type = ::get_alpha_type(colors + i, 2, this->force_opaque || this->target_requires_palette_);
        }

        if (alpha_type != ::alpha_type::invisible)
        {
            ffdu::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ffdu::geometry_bucket_type::lines_alpha : ffdu::geometry_bucket_type::lines;
            this->add_geometry(&input, bucket_type, input.depth);
        }
    }
#endif
}

void ffdu::draw_device_base::draw_triangles(std::span<const ff::dxgi::endpoint_t> points)
{
#if 0
    ff::vertex::triangle_geometry input;
    input.matrix_index = (this->world_matrix_index == ::INVALID_INDEX) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.depth = this->nudge_depth(this->force_no_overlap ? ffdu::last_depth_type::instance_no_overlap : ffdu::last_depth_type::instance);

    for (size_t i = 0; i < count; i++, points += 3, colors += 3)
    {
        std::memcpy(input.position, points, sizeof(input.position));
        std::memcpy(input.color, colors, sizeof(input.color));

        ::alpha_type alpha_type = ::get_alpha_type(colors, 3, this->force_opaque || this->target_requires_palette_);
        if (alpha_type != ::alpha_type::invisible)
        {
            ffdu::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ffdu::geometry_bucket_type::triangles_alpha : ffdu::geometry_bucket_type::triangles;
            this->add_geometry(&input, bucket_type, input.depth);
        }
    }
#endif
}

void ffdu::draw_device_base::draw_rectangle(const ff::rect_float& rect, const ff::color& color, std::optional<float> thickness)
{
#if 0
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
#endif
}

void ffdu::draw_device_base::draw_circle(const ff::dxgi::endpoint_t& pos, std::optional<float> thickness, const ff::color* outside_color)
{
#if 0
    ff::vertex::circle_geometry input;
    input.matrix_index = (this->world_matrix_index == ::INVALID_INDEX) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.position.x = center.x;
    input.position.y = center.y;
    input.position.z = this->nudge_depth(this->force_no_overlap ? ffdu::last_depth_type::instance_no_overlap : ffdu::last_depth_type::instance);
    input.radius = std::abs(radius);
    input.thickness = pixel_thickness ? -std::abs(thickness) : std::min(std::abs(thickness), input.radius);
    input.inside_color = inside_color;
    input.outside_color = outside_color;

    ::alpha_type alpha_type = ::get_alpha_type(&input.inside_color, 2, this->force_opaque || this->target_requires_palette_);
    if (alpha_type != ::alpha_type::invisible)
    {
        ffdu::geometry_bucket_type bucket_type = (alpha_type == ::alpha_type::transparent) ? ffdu::geometry_bucket_type::circles_alpha : ffdu::geometry_bucket_type::circles;
        this->add_geometry(&input, bucket_type, input.position.z);
    }
#endif
}

ff::matrix_stack& ffdu::draw_device_base::world_matrix_stack()
{
    return this->world_matrix_stack_;
}

void ffdu::draw_device_base::push_palette(ff::dxgi::palette_base* palette)
{
    assert(palette);

    if (!this->target_requires_palette_)
    {
        this->palette_stack.push_back(palette);
        this->palette_index = ::INVALID_INDEX;
    }

    this->push_palette_remap(palette->remap());
}

void ffdu::draw_device_base::pop_palette()
{
    if (!this->target_requires_palette_)
    {
        assert(this->palette_stack.size() > 1);
        this->palette_stack.pop_back();
        this->palette_index = ::INVALID_INDEX;
    }

    this->pop_palette_remap();
}

void ffdu::draw_device_base::push_palette_remap(ff::dxgi::remap_t remap)
{
    this->palette_remap_stack.push_back(remap.hash ? remap : ::default_palette_remap());
    this->palette_remap_index = ::INVALID_INDEX;
}

void ffdu::draw_device_base::pop_palette_remap()
{
    assert(this->palette_remap_stack.size() > 1);
    this->palette_remap_stack.pop_back();
    this->palette_remap_index = ::INVALID_INDEX;
}

void ffdu::draw_device_base::push_no_overlap()
{
    this->force_no_overlap++;
}

void ffdu::draw_device_base::pop_no_overlap()
{
    assert(this->force_no_overlap > 0);

    if (!--this->force_no_overlap && this->last_depth_type == ffdu::last_depth_type::instance_no_overlap)
    {
        this->last_depth_type = ffdu::last_depth_type::instance;
    }
}

void ffdu::draw_device_base::push_opaque()
{
    this->force_opaque++;
}

void ffdu::draw_device_base::pop_opaque()
{
    assert(this->force_opaque > 0);
    this->force_opaque--;
}

void ffdu::draw_device_base::push_pre_multiplied_alpha()
{
    if (!this->force_pre_multiplied_alpha)
    {
        this->flush();
    }

    this->force_pre_multiplied_alpha++;
}

void ffdu::draw_device_base::pop_pre_multiplied_alpha()
{
    assert(this->force_pre_multiplied_alpha > 0);

    if (this->force_pre_multiplied_alpha == 1)
    {
        this->flush();
    }

    this->force_pre_multiplied_alpha--;
}

void ffdu::draw_device_base::push_custom_context(ff::dxgi::draw_base::custom_context_func&& func)
{
    this->flush();
    this->custom_context_stack.push_back(std::move(func));
}

void ffdu::draw_device_base::pop_custom_context()
{
    assert(this->custom_context_stack.size());

    this->flush();
    this->custom_context_stack.pop_back();
}

void ffdu::draw_device_base::push_sampler_linear_filter(bool linear_filter)
{
    if (linear_filter != this->linear_sampler() && this->flush_for_sampler_change())
    {
        this->flush();
    }

    this->sampler_stack.push_back(linear_filter);
}

void ffdu::draw_device_base::pop_sampler_linear_filter()
{
    const size_t size = this->sampler_stack.size();
    assert(size > 1);

    if (this->sampler_stack[size - 1] != this->sampler_stack[size - 2] && this->flush_for_sampler_change())
    {
        this->flush();
    }

    this->sampler_stack.pop_back();
}

ff::dxgi::device_child_base* ffdu::draw_device_base::as_device_child()
{
    return this;
}

bool ffdu::draw_device_base::internal_valid() const
{
    return this->state != draw_device_base::state_t::invalid;
}

bool ffdu::draw_device_base::linear_sampler() const
{
    return this->sampler_stack.back();
}

bool ffdu::draw_device_base::target_requires_palette() const
{
    return this->target_requires_palette_;
}

bool ffdu::draw_device_base::pre_multiplied_alpha() const
{
    return this->force_pre_multiplied_alpha > 0;
}

static void draw_ptr_deleter(ff::dxgi::draw_base* draw)
{
    draw->end_draw();
}

ff::dxgi::draw_ptr ffdu::draw_device_base::internal_begin_draw(
    ff::dxgi::command_context_base& context,
    ff::dxgi::target_base& target,
    ff::dxgi::depth_base* depth,
    const ff::rect_float& view_rect,
    const ff::rect_float& world_rect,
    ff::dxgi::draw_options options)
{
    this->end_draw();

    bool ignore_rotation = ff::flags::has(options, ff::dxgi::draw_options::ignore_rotation);

    if (::setup_view_matrix(target, view_rect, world_rect, this->view_matrix, ignore_rotation) &&
        (this->command_context_ = this->internal_setup(context, target, depth, view_rect, ignore_rotation)) != nullptr)
    {
        this->init_vs_constants_buffer_0(target, view_rect, world_rect);
        this->target_requires_palette_ = ff::dxgi::palette_format(target.format());
        this->force_pre_multiplied_alpha = ff::flags::has(options, ff::dxgi::draw_options::pre_multiplied_alpha) && ff::dxgi::supports_pre_multiplied_alpha(target.format()) ? 1 : 0;
        this->state = draw_device_base::state_t::drawing;

        return { this, ::draw_ptr_deleter };
    }

    return { nullptr, ::draw_ptr_deleter };
}

bool ffdu::draw_device_base::reset()
{
    this->destroy();

    // Geometry buckets
    for (auto& i : this->instance_buckets)
    {
        i.reset();
    }

    // Palette
    this->palette_stack.push_back(nullptr);
    this->palette_texture = this->create_texture(ff::point_size(ff::dxgi::palette_size, ffdu::MAX_PALETTES), DXGI_FORMAT_R8G8B8A8_UNORM);

    this->palette_remap_stack.push_back(::default_palette_remap());
    this->palette_remap_texture = this->create_texture(ff::point_size(ff::dxgi::palette_size, ffdu::MAX_PALETTE_REMAPS), DXGI_FORMAT_R8_UINT);

    // States
    this->sampler_stack.push_back(false);

    this->internal_reset();

    this->state = draw_device_base::state_t::valid;
    return true;
}

void ffdu::draw_device_base::destroy()
{
    this->state = draw_device_base::state_t::invalid;

    this->internal_destroy();

    this->vs_constants_0 = ffdu::vs_constants_0{};
    this->vs_constants_1 = ffdu::vs_constants_1{};
    this->ps_constants_0 = ffdu::ps_constants_0{};

    this->sampler_stack.clear();
    this->custom_context_stack.clear();

    this->view_matrix = ff::matrix_identity_4x4();
    this->world_matrix_stack_.reset();
    this->world_matrix_to_index.clear();
    this->world_matrix_index = ::INVALID_INDEX;

    std::memset(this->textures.data(), 0, ff::array_byte_size(this->textures));
    std::memset(this->textures_using_palette.data(), 0, ff::array_byte_size(this->textures_using_palette));
    this->texture_count = 0;
    this->textures_using_palette_count = 0;

    std::memset(this->palette_texture_hashes.data(), 0, ff::array_byte_size(this->palette_texture_hashes));
    this->palette_stack.clear();
    this->palette_to_index.clear();
    this->palette_texture = nullptr;
    this->palette_index = ::INVALID_INDEX;

    std::memset(this->palette_remap_texture_hashes.data(), 0, ff::array_byte_size(this->palette_remap_texture_hashes));
    this->palette_remap_stack.clear();
    this->palette_remap_to_index.clear();
    this->palette_remap_texture = nullptr;
    this->palette_remap_index = ::INVALID_INDEX;

    this->transparent_instances.clear();
    this->last_depth_type = ffdu::last_depth_type::none;
    this->draw_depth = 0;
    this->force_no_overlap = 0;
    this->force_opaque = 0;
    this->force_pre_multiplied_alpha = 0;

    for (auto& bucket : this->instance_buckets)
    {
        bucket.reset();
    }
}

void ffdu::draw_device_base::flush(bool end_draw)
{
    if (this->last_depth_type != ffdu::last_depth_type::none && this->create_instance_buffer())
    {
        this->internal_flush_begin(this->command_context_);
        this->update_vs_constants_buffer_0();
        this->update_vs_constants_buffer_1();
        this->update_ps_constants_buffer_0();

        if (this->target_requires_palette_ || this->textures_using_palette_count)
        {
            this->update_palette_texture(*this->command_context_, this->textures_using_palette_count,
                *this->palette_texture, this->palette_texture_hashes.data(), this->palette_to_index,
                *this->palette_remap_texture, this->palette_remap_texture_hashes.data(), this->palette_remap_to_index);
        }

        this->apply_shader_input(*this->command_context_,
            this->texture_count, this->textures.data(),
            this->textures_using_palette_count, this->textures_using_palette.data(),
            *this->palette_texture, *this->palette_remap_texture);

        this->draw_opaque_instances();
        this->draw_transparent_instances();
        this->internal_flush_end(this->command_context_);

        // Reset draw data

        this->world_matrix_to_index.clear();
        this->world_matrix_index = ::INVALID_INDEX;

        this->palette_to_index.clear();
        this->palette_index = ::INVALID_INDEX;
        this->palette_remap_to_index.clear();
        this->palette_remap_index = ::INVALID_INDEX;

        this->texture_count = 0;
        this->textures_using_palette_count = 0;

        this->transparent_instances.clear();
        this->last_depth_type = ffdu::last_depth_type::none;

        this->command_context_ = this->internal_flush(this->command_context_, end_draw);
    }
    else if (end_draw)
    {
        this->command_context_ = this->internal_flush(this->command_context_, end_draw);
    }
}

void ffdu::draw_device_base::matrix_changing(const ff::matrix_stack& matrix_stack)
{
    this->world_matrix_index = ::INVALID_INDEX;
}

void ffdu::draw_device_base::init_vs_constants_buffer_0(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect)
{
    ff::point_float view_size = view_rect.size() / static_cast<float>(target.size().dpi_scale);
    this->vs_constants_0.view_scale = world_rect.size() / view_size;
}

void ffdu::draw_device_base::update_vs_constants_buffer_0()
{
    this->vs_constants_0.projection = this->view_matrix;
    this->vs_constants_buffer_0().update(*this->command_context_, &this->vs_constants_0, sizeof(ffdu::vs_constants_0));
}

void ffdu::draw_device_base::update_vs_constants_buffer_1()
{
    // Build up model matrix array
    size_t world_matrix_count = this->world_matrix_to_index.size();
    this->vs_constants_1.model.resize(world_matrix_count);

    for (const auto& iter : this->world_matrix_to_index)
    {
        this->vs_constants_1.model[iter.second] = iter.first;
    }

    size_t buffer_size = sizeof(DirectX::XMFLOAT4X4) * (ff::constants::debug_build ? ffdu::MAX_TRANSFORM_MATRIXES : world_matrix_count);
    this->vs_constants_buffer_1().update(*this->command_context_, this->vs_constants_1.model.data(), ff::vector_byte_size(this->vs_constants_1.model), buffer_size);
}

void ffdu::draw_device_base::update_ps_constants_buffer_0()
{
    if (this->textures_using_palette_count)
    {
        for (size_t i = 0; i < this->textures_using_palette_count; i++)
        {
            ff::rect_float& rect = this->ps_constants_0.texture_palette_sizes[i];
            ff::point_float size = this->textures_using_palette[i]->view_texture()->size().cast<float>();
            rect.left = size.x;
            rect.top = size.y;
        }

        this->ps_constants_buffer_0().update(*this->command_context_, &this->ps_constants_0, sizeof(ffdu::ps_constants_0));
    }
}

bool ffdu::draw_device_base::create_instance_buffer()
{
    size_t byte_size = 0;

    for (ffdu::instance_bucket& bucket : this->instance_buckets)
    {
        byte_size = ff::math::round_up(byte_size, bucket.item_size());
        bucket.render_start(byte_size / bucket.item_size());
        byte_size += bucket.byte_size();
    }

    void* buffer_data = this->instance_buffer().map(*this->command_context_, byte_size);
    if (buffer_data)
    {
        for (ffdu::instance_bucket& bucket : this->instance_buckets)
        {
            if (bucket.render_count())
            {
                ::memcpy(reinterpret_cast<uint8_t*>(buffer_data) + bucket.render_start() * bucket.item_size(), bucket.data(), bucket.byte_size());
                bucket.clear_items();
            }
        }

        this->instance_buffer().unmap();

        return true;
    }

    assert(!byte_size);
    return false;
}

void ffdu::draw_device_base::draw_opaque_instances()
{
    const size_t custom_size = this->custom_context_stack.size();
    const ff::dxgi::draw_base::custom_context_func* custom_func = custom_size ? &this->custom_context_stack[custom_size - 1] : nullptr;
    this->apply_opaque_state(*this->command_context_);

    for (ffdu::instance_bucket& bucket : this->instance_buckets)
    {
        if (bucket.bucket_type() >= ffdu::instance_bucket_type::first_transparent)
        {
            break;
        }

        if (bucket.render_count() && this->apply_instance_state(*this->command_context_, bucket))
        {
            if (!custom_func || (*custom_func)(*this->command_context_, bucket.item_type(), true))
            {
                this->draw(*this->command_context_, bucket.render_count(), bucket.render_start());
            }
        }
    }
}

void ffdu::draw_device_base::draw_transparent_instances()
{
    const size_t alpha_geometry_size = this->transparent_instances.size();
    if (alpha_geometry_size)
    {
        const size_t custom_size = this->custom_context_stack.size();
        const ff::dxgi::draw_base::custom_context_func* custom_func = custom_size ? &this->custom_context_stack[custom_size - 1] : nullptr;
        this->apply_transparent_state(*this->command_context_);

        for (size_t i = 0; i < alpha_geometry_size; )
        {
            const ffdu::transparent_instance_entry& entry = this->transparent_instances[i];
            size_t geometry_count = 1;

            for (i++; i < alpha_geometry_size; i++, geometry_count++)
            {
                const ffdu::transparent_instance_entry& entry2 = this->transparent_instances[i];
                if (entry2.bucket != entry.bucket ||
                    entry2.depth != entry.depth ||
                    entry2.index != entry.index + geometry_count)
                {
                    break;
                }
            }

            if (this->apply_instance_state(*this->command_context_, *entry.bucket))
            {
                if (!custom_func || (*custom_func)(*this->command_context_, entry.bucket->item_type(), false))
                {
                    this->draw(*this->command_context_, geometry_count, entry.bucket->render_start() + entry.index);
                }
            }
        }
    }
}

float ffdu::draw_device_base::nudge_depth(ffdu::last_depth_type depth_type)
{
    if (depth_type != ffdu::last_depth_type::instance_no_overlap || this->last_depth_type != depth_type)
    {
        this->draw_depth += ffdu::RENDER_DEPTH_DELTA;
    }

    this->last_depth_type = depth_type;
    return this->draw_depth;
}

uint32_t ffdu::draw_device_base::get_world_matrix_index()
{
    uint32_t index = this->get_world_matrix_index_no_flush();
    if (index == ::INVALID_INDEX)
    {
        this->flush();
        index = this->get_world_matrix_index_no_flush();
    }

    return index;
}

uint32_t ffdu::draw_device_base::get_world_matrix_index_no_flush()
{
    if (this->world_matrix_index == ::INVALID_INDEX)
    {
        DirectX::XMFLOAT4X4 wm;
        DirectX::XMStoreFloat4x4(&wm, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&this->world_matrix_stack_.matrix())));
        auto iter = this->world_matrix_to_index.find(wm);

        if (iter == this->world_matrix_to_index.cend() && this->world_matrix_to_index.size() != ffdu::MAX_TRANSFORM_MATRIXES)
        {
            iter = this->world_matrix_to_index.try_emplace(wm, static_cast<uint32_t>(this->world_matrix_to_index.size())).first;
        }

        if (iter != this->world_matrix_to_index.cend())
        {
            this->world_matrix_index = iter->second;
        }
    }

    return this->world_matrix_index;
}

uint32_t ffdu::draw_device_base::get_texture_index_no_flush(ff::dxgi::texture_view_base& texture_view, bool use_palette)
{
    if (use_palette)
    {
        uint32_t palette_index = (this->palette_index == ::INVALID_INDEX) ? this->get_palette_index_no_flush() : this->palette_index;
        if (palette_index == ::INVALID_INDEX)
        {
            return ::INVALID_INDEX;
        }

        uint32_t palette_remap_index = (this->palette_remap_index == ::INVALID_INDEX) ? this->get_palette_remap_index_no_flush() : this->palette_remap_index;
        if (palette_remap_index == ::INVALID_INDEX)
        {
            return ::INVALID_INDEX;
        }

        uint32_t texture_index = ::INVALID_INDEX;

        for (size_t i = this->textures_using_palette_count; i != 0; i--)
        {
            if (this->textures_using_palette[i - 1] == &texture_view)
            {
                texture_index = static_cast<uint32_t>(i - 1);
                break;
            }
        }

        if (texture_index == ::INVALID_INDEX)
        {
            if (this->textures_using_palette_count == ffdu::MAX_TEXTURES_USING_PALETTE)
            {
                return ::INVALID_INDEX;
            }

            this->textures_using_palette[this->textures_using_palette_count] = &texture_view;
            texture_index = static_cast<uint32_t>(this->textures_using_palette_count++);
        }

        return texture_index | (palette_index << 8) | (palette_remap_index << 16);
    }
    else
    {
        uint32_t palette_remap_index = 0;
        if (this->target_requires_palette_)
        {
            palette_remap_index = (this->palette_remap_index == ::INVALID_INDEX) ? this->get_palette_remap_index_no_flush() : this->palette_remap_index;
            if (palette_remap_index == ::INVALID_INDEX)
            {
                return ::INVALID_INDEX;
            }
        }

        uint32_t texture_index = ::INVALID_INDEX;
        uint32_t sampler_index = static_cast<uint32_t>(this->linear_sampler());

        for (size_t i = this->texture_count; i != 0; i--)
        {
            if (this->textures[i - 1] == &texture_view)
            {
                texture_index = static_cast<uint32_t>(i - 1);
                break;
            }
        }

        if (texture_index == ::INVALID_INDEX)
        {
            if (this->texture_count == ffdu::MAX_TEXTURES)
            {
                return ::INVALID_INDEX;
            }

            this->textures[this->texture_count] = &texture_view;
            texture_index = static_cast<uint32_t>(this->texture_count++);
        }

        return texture_index | (sampler_index << 8) | (palette_remap_index << 16);
    }
}

uint32_t ffdu::draw_device_base::get_palette_index_no_flush()
{
    if (this->palette_index == ::INVALID_INDEX)
    {
        if (this->target_requires_palette_)
        {
            // Not converting palette to RGBA, so don't use a palette
            this->palette_index = 0;
        }
        else
        {
            ff::dxgi::palette_base* palette = this->palette_stack.back();
            size_t palette_hash = palette ? palette->data()->row_hash(palette->current_row()) : 0;
            auto iter = this->palette_to_index.find(palette_hash);

            if (iter == this->palette_to_index.cend() && this->palette_to_index.size() != ffdu::MAX_PALETTES)
            {
                iter = this->palette_to_index.try_emplace(palette_hash, std::make_pair(palette, static_cast<uint32_t>(this->palette_to_index.size()))).first;
            }

            if (iter != this->palette_to_index.cend())
            {
                this->palette_index = iter->second.second;
            }
        }
    }

    return this->palette_index;
}

uint32_t ffdu::draw_device_base::get_palette_remap_index_no_flush()
{
    if (this->palette_remap_index == ::INVALID_INDEX)
    {
        auto& remap_pair = this->palette_remap_stack.back();
        auto iter = this->palette_remap_to_index.find(remap_pair.hash);

        if (iter == this->palette_remap_to_index.cend() && this->palette_remap_to_index.size() != ffdu::MAX_PALETTE_REMAPS)
        {
            iter = this->palette_remap_to_index.try_emplace(remap_pair.hash, std::make_pair(remap_pair, static_cast<uint32_t>(this->palette_remap_to_index.size()))).first;
        }

        if (iter != this->palette_remap_to_index.cend())
        {
            this->palette_remap_index = iter->second.second;
        }
    }

    return this->palette_remap_index;
}

int ffdu::draw_device_base::remap_palette_index(int color) const
{
    return this->palette_remap_stack.back().remap[color];
}

uint32_t ffdu::draw_device_base::get_world_matrix_and_texture_index(ff::dxgi::texture_view_base& texture_view, bool use_palette)
{
    uint32_t model_index = (this->world_matrix_index == ::INVALID_INDEX) ? this->get_world_matrix_index_no_flush() : this->world_matrix_index;
    uint32_t texture_index = this->get_texture_index_no_flush(texture_view, use_palette);

    if (model_index == ::INVALID_INDEX || texture_index == ::INVALID_INDEX)
    {
        this->flush();
        return this->get_world_matrix_and_texture_index(texture_view, use_palette);
    }

    return (model_index << 24) | texture_index;
}

void* ffdu::draw_device_base::add_instance(const void* data, ffdu::instance_bucket_type bucket_type, float depth)
{
    ffdu::instance_bucket& bucket = this->get_instance_bucket(bucket_type);

    if (bucket_type >= ffdu::instance_bucket_type::first_transparent)
    {
        assert(!this->force_opaque);
        this->transparent_instances.emplace_back(&bucket, bucket.count(), depth);
    }

    return bucket.add(data);
}

ffdu::instance_bucket& ffdu::draw_device_base::get_instance_bucket(ffdu::instance_bucket_type type)
{
    return this->instance_buckets[static_cast<size_t>(type)];
}

void ffdu::draw_device_base::internal_flush_begin(ff::dxgi::command_context_base* context)
{
}

void ffdu::draw_device_base::internal_flush_end(ff::dxgi::command_context_base* context)
{
}
