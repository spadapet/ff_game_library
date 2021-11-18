#include "pch.h"
#include "buffer_base.h"
#include "depth_base.h"
#include "draw_util.h"
#include "format_util.h"
#include "matrix.h"
#include "operators.h"
#include "palette_data_base.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "target_base.h"
#include "texture_base.h"
#include "texture_view_base.h"
#include "transform.h"
#include "vertex.h"

const std::array<uint8_t, ff::dxgi::palette_size>& ff::dxgi::draw_util::default_palette_remap()
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

    return value;
}

size_t ff::dxgi::draw_util::default_palette_remap_hash()
{
    static size_t value = ff::stable_hash_bytes(
        ff::dxgi::draw_util::default_palette_remap().data(),
        ff::array_byte_size(ff::dxgi::draw_util::default_palette_remap()));

    return value;
}

ff::dxgi::draw_util::geometry_bucket::geometry_bucket(
    ff::dxgi::draw_util::geometry_bucket_type bucket_type,
    const std::type_info& item_type,
    size_t item_size,
    size_t item_align)
    : bucket_type_(bucket_type)
    , item_type_(&item_type)
    , item_size_(item_size)
    , item_align(item_align)
    , render_start_(0)
    , render_count_(0)
    , data_start(nullptr)
    , data_cur(nullptr)
    , data_end(nullptr)
{}

ff::dxgi::draw_util::geometry_bucket::geometry_bucket(ff::dxgi::draw_util::geometry_bucket&& rhs) noexcept
    : bucket_type_(rhs.bucket_type_)
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

ff::dxgi::draw_util::geometry_bucket::~geometry_bucket()
{
    ::_aligned_free(this->data_start);
}

void ff::dxgi::draw_util::geometry_bucket::reset()
{
    ::_aligned_free(this->data_start);
    this->data_start = nullptr;
    this->data_cur = nullptr;
    this->data_end = nullptr;
}

void* ff::dxgi::draw_util::geometry_bucket::add(const void* data)
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

size_t ff::dxgi::draw_util::geometry_bucket::item_size() const
{
    return this->item_size_;
}

const std::type_info& ff::dxgi::draw_util::geometry_bucket::item_type() const
{
    return *this->item_type_;
}

ff::dxgi::draw_util::geometry_bucket_type ff::dxgi::draw_util::geometry_bucket::bucket_type() const
{
    return this->bucket_type_;
}

size_t ff::dxgi::draw_util::geometry_bucket::count() const
{
    return (this->data_cur - this->data_start) / this->item_size_;
}

void ff::dxgi::draw_util::geometry_bucket::clear_items()
{
    this->data_cur = this->data_start;
}

size_t ff::dxgi::draw_util::geometry_bucket::byte_size() const
{
    return this->data_cur - this->data_start;
}

const uint8_t* ff::dxgi::draw_util::geometry_bucket::data() const
{
    return this->data_start;
}

void ff::dxgi::draw_util::geometry_bucket::render_start(size_t start)
{
    this->render_start_ = start;
    this->render_count_ = this->count();
}

size_t ff::dxgi::draw_util::geometry_bucket::render_start() const
{
    return this->render_start_;
}

size_t ff::dxgi::draw_util::geometry_bucket::render_count() const
{
    return this->render_count_;
}

ff::dxgi::draw_util::alpha_type ff::dxgi::draw_util::get_alpha_type(const DirectX::XMFLOAT4& color, bool force_opaque)
{
    if (color.w == 0)
    {
        return ff::dxgi::draw_util::alpha_type::invisible;
    }

    if (color.w == 1 || force_opaque)
    {
        return ff::dxgi::draw_util::alpha_type::opaque;
    }

    return ff::dxgi::draw_util::alpha_type::transparent;
}

ff::dxgi::draw_util::alpha_type ff::dxgi::draw_util::get_alpha_type(const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    ff::dxgi::draw_util::alpha_type type = ff::dxgi::draw_util::alpha_type::invisible;

    for (size_t i = 0; i < count; i++)
    {
        switch (ff::dxgi::draw_util::get_alpha_type(colors[i], force_opaque))
        {
            case ff::dxgi::draw_util::alpha_type::opaque:
                type = ff::dxgi::draw_util::alpha_type::opaque;
                break;

            case ff::dxgi::draw_util::alpha_type::transparent:
                return ff::dxgi::draw_util::alpha_type::transparent;
        }
    }

    return type;
}

ff::dxgi::draw_util::alpha_type ff::dxgi::draw_util::get_alpha_type(const ff::dxgi::sprite_data& data, const DirectX::XMFLOAT4& color, bool force_opaque)
{
    switch (ff::dxgi::draw_util::get_alpha_type(color, force_opaque))
    {
        case ff::dxgi::draw_util::alpha_type::transparent:
            return ff::flags::has(data.type(), ff::dxgi::sprite_type::palette) ? alpha_type::opaque : alpha_type::transparent;

        case ff::dxgi::draw_util::alpha_type::opaque:
            return (ff::flags::has(data.type(), ff::dxgi::sprite_type::transparent) && !force_opaque)
                ? ff::dxgi::draw_util::alpha_type::transparent
                : ff::dxgi::draw_util::alpha_type::opaque;

        default:
            return ff::dxgi::draw_util::alpha_type::invisible;
    }
}

ff::dxgi::draw_util::alpha_type ff::dxgi::draw_util::get_alpha_type(const ff::dxgi::sprite_data** datas, const DirectX::XMFLOAT4* colors, size_t count, bool force_opaque)
{
    ff::dxgi::draw_util::alpha_type type = ff::dxgi::draw_util::alpha_type::invisible;

    for (size_t i = 0; i < count; i++)
    {
        switch (ff::dxgi::draw_util::get_alpha_type(*datas[i], colors[i], force_opaque))
        {
            case ff::dxgi::draw_util::alpha_type::opaque:
                type = ff::dxgi::draw_util::alpha_type::opaque;
                break;

            case ff::dxgi::draw_util::alpha_type::transparent:
                return ff::dxgi::draw_util::alpha_type::transparent;
        }
    }

    return type;
}

ff::rect_float ff::dxgi::draw_util::get_rotated_view_rect(ff::dxgi::target_base& target, const ff::rect_float& view_rect)
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

DirectX::XMMATRIX ff::dxgi::draw_util::get_view_matrix(const ff::rect_float& world_rect)
{
    return DirectX::XMMatrixOrthographicOffCenterLH(
        world_rect.left,
        world_rect.right,
        world_rect.bottom,
        world_rect.top,
        0, ff::dxgi::draw_util::MAX_RENDER_DEPTH);
}

DirectX::XMMATRIX ff::dxgi::draw_util::get_orientation_matrix(ff::dxgi::target_base& target, const ff::rect_float& view_rect, ff::point_float world_center)
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

bool ff::dxgi::draw_util::setup_view_matrix(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect, DirectX::XMFLOAT4X4& view_matrix)
{
    if (world_rect.width() != 0 && world_rect.height() != 0 && view_rect.width() > 0 && view_rect.height() > 0)
    {
        DirectX::XMMATRIX unoriented_view_matrix = ff::dxgi::draw_util::get_view_matrix(world_rect);
        DirectX::XMMATRIX orientation_matrix = ff::dxgi::draw_util::get_orientation_matrix(target, view_rect, world_rect.center());
        DirectX::XMStoreFloat4x4(&view_matrix, DirectX::XMMatrixTranspose(orientation_matrix * unoriented_view_matrix));

        return true;
    }

    return false;
}

ff::dxgi::draw_util::draw_device_base::draw_device_base()
    : state(draw_device_base::state_t::invalid)
    , command_context_(nullptr)
    , world_matrix_stack_changing_connection(this->world_matrix_stack_.matrix_changing().connect(std::bind(&draw_device_base::matrix_changing, this, std::placeholders::_1)))
    , geometry_buckets
{
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::line_geometry, ff::dxgi::draw_util::geometry_bucket_type::lines>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::circle_geometry, ff::dxgi::draw_util::geometry_bucket_type::circles>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::triangle_geometry, ff::dxgi::draw_util::geometry_bucket_type::triangles>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::sprite_geometry, ff::dxgi::draw_util::geometry_bucket_type::sprites>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::sprite_geometry, ff::dxgi::draw_util::geometry_bucket_type::palette_sprites>(),

    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::line_geometry, ff::dxgi::draw_util::geometry_bucket_type::lines_alpha>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::circle_geometry, ff::dxgi::draw_util::geometry_bucket_type::circles_alpha>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::triangle_geometry, ff::dxgi::draw_util::geometry_bucket_type::triangles_alpha>(),
    ff::dxgi::draw_util::geometry_bucket::create<ff::dxgi::vertex::sprite_geometry, ff::dxgi::draw_util::geometry_bucket_type::sprites_alpha>(),
}
{}

ff::dxgi::draw_util::draw_device_base::~draw_device_base()
{
    assert(this->state != draw_device_base::state_t::drawing);
}

void ff::dxgi::draw_util::draw_device_base::end_draw()
{
    if (this->state == state_t::drawing)
    {
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
}

void ff::dxgi::draw_util::draw_device_base::draw_sprite(const ff::dxgi::sprite_data& sprite, const ff::dxgi::transform& transform)
{
    ff::dxgi::draw_util::alpha_type alpha_type = ff::dxgi::draw_util::get_alpha_type(sprite, transform.color, this->force_opaque || this->target_requires_palette);
    if (alpha_type != ff::dxgi::draw_util::alpha_type::invisible && sprite.view())
    {
        bool use_palette = ff::flags::has(sprite.type(), ff::dxgi::sprite_type::palette);
        ff::dxgi::draw_util::geometry_bucket_type bucket_type = (alpha_type == ff::dxgi::draw_util::alpha_type::transparent && !this->target_requires_palette)
            ? (use_palette ? ff::dxgi::draw_util::geometry_bucket_type::palette_sprites : ff::dxgi::draw_util::geometry_bucket_type::sprites_alpha)
            : (use_palette ? ff::dxgi::draw_util::geometry_bucket_type::palette_sprites : ff::dxgi::draw_util::geometry_bucket_type::sprites);

        float depth = this->nudge_depth(this->force_no_overlap ? ff::dxgi::draw_util::last_depth_type::sprite_no_overlap : ff::dxgi::draw_util::last_depth_type::sprite);
        ff::dxgi::vertex::sprite_geometry& input = *reinterpret_cast<ff::dxgi::vertex::sprite_geometry*>(this->add_geometry(nullptr, bucket_type, depth));

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

void ff::dxgi::draw_util::draw_device_base::draw_line_strip(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count, float thickness, bool pixel_thickness)
{
    this->draw_line_strip(points, count, colors, count, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_line_strip(const ff::point_float* points, size_t count, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness)
{
    this->draw_line_strip(points, count, &color, 1, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_line(const ff::point_float& start, const ff::point_float& end, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness)
{
    const ff::point_float points[2] = { start, end };
    this->draw_line_strip(points, 2, color, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4* colors)
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

void ff::dxgi::draw_util::draw_device_base::draw_filled_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color)
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

void ff::dxgi::draw_util::draw_device_base::draw_filled_triangles(const ff::point_float* points, const DirectX::XMFLOAT4* colors, size_t count)
{
    ff::dxgi::vertex::triangle_geometry input;
    input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.depth = this->nudge_depth(this->force_no_overlap ? ff::dxgi::draw_util::last_depth_type::triangle_no_overlap : ff::dxgi::draw_util::last_depth_type::triangle);

    for (size_t i = 0; i < count; i++, points += 3, colors += 3)
    {
        std::memcpy(input.position, points, sizeof(input.position));
        std::memcpy(input.color, colors, sizeof(input.color));

        ff::dxgi::draw_util::alpha_type alpha_type = ff::dxgi::draw_util::get_alpha_type(colors, 3, this->force_opaque || this->target_requires_palette);
        if (alpha_type != ff::dxgi::draw_util::alpha_type::invisible)
        {
            ff::dxgi::draw_util::geometry_bucket_type bucket_type = (alpha_type == ff::dxgi::draw_util::alpha_type::transparent) ? ff::dxgi::draw_util::geometry_bucket_type::triangles_alpha : ff::dxgi::draw_util::geometry_bucket_type::triangles;
            this->add_geometry(&input, bucket_type, input.depth);
        }
    }
}

void ff::dxgi::draw_util::draw_device_base::draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color)
{
    this->draw_outline_circle(center, radius, color, color, std::abs(radius), false);
}

void ff::dxgi::draw_util::draw_device_base::draw_filled_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color)
{
    this->draw_outline_circle(center, radius, inside_color, outside_color, std::abs(radius), false);
}

void ff::dxgi::draw_util::draw_device_base::draw_outline_rectangle(const ff::rect_float& rect, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness)
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

void ff::dxgi::draw_util::draw_device_base::draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& color, float thickness, bool pixel_thickness)
{
    this->draw_outline_circle(center, radius, color, color, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_outline_circle(const ff::point_float& center, float radius, const DirectX::XMFLOAT4& inside_color, const DirectX::XMFLOAT4& outside_color, float thickness, bool pixel_thickness)
{
    ff::dxgi::vertex::circle_geometry input;
    input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.position.x = center.x;
    input.position.y = center.y;
    input.position.z = this->nudge_depth(this->force_no_overlap ? ff::dxgi::draw_util::last_depth_type::circle_no_overlap : ff::dxgi::draw_util::last_depth_type::circle);
    input.radius = std::abs(radius);
    input.thickness = pixel_thickness ? -std::abs(thickness) : std::min(std::abs(thickness), input.radius);
    input.inside_color = inside_color;
    input.outside_color = outside_color;

    ff::dxgi::draw_util::alpha_type alpha_type = ff::dxgi::draw_util::get_alpha_type(&input.inside_color, 2, this->force_opaque || this->target_requires_palette);
    if (alpha_type != ff::dxgi::draw_util::alpha_type::invisible)
    {
        ff::dxgi::draw_util::geometry_bucket_type bucket_type = (alpha_type == ff::dxgi::draw_util::alpha_type::transparent) ? ff::dxgi::draw_util::geometry_bucket_type::circles_alpha : ff::dxgi::draw_util::geometry_bucket_type::circles;
        this->add_geometry(&input, bucket_type, input.position.z);
    }
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_line_strip(const ff::point_float* points, const int* colors, size_t count, float thickness, bool pixel_thickness)
{
    ff::stack_vector<DirectX::XMFLOAT4, 64> colors2;
    colors2.resize(count);

    for (size_t i = 0; i != colors2.size(); i++)
    {
        ff::dxgi::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
    }

    this->draw_line_strip(points, count, colors2.data(), count, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_line_strip(const ff::point_float* points, size_t count, int color, float thickness, bool pixel_thickness)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_line_strip(points, count, &color2, 1, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_line(const ff::point_float& start, const ff::point_float& end, int color, float thickness, bool pixel_thickness)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_line(start, end, color2, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_filled_rectangle(const ff::rect_float& rect, const int* colors)
{
    std::array<DirectX::XMFLOAT4, 4> colors2;

    for (size_t i = 0; i != colors2.size(); i++)
    {
        ff::dxgi::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
    }

    this->draw_filled_rectangle(rect, colors2.data());
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_filled_rectangle(const ff::rect_float& rect, int color)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_filled_rectangle(rect, color2);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_filled_triangles(const ff::point_float* points, const int* colors, size_t count)
{
    ff::stack_vector<DirectX::XMFLOAT4, 64 * 3> colors2;
    colors2.resize(count * 3);

    for (size_t i = 0; i != colors2.size(); i++)
    {
        ff::dxgi::palette_index_to_color(this->remap_palette_index(colors[i]), colors2[i]);
    }

    this->draw_filled_triangles(points, colors2.data(), count);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_filled_circle(const ff::point_float& center, float radius, int color)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_filled_circle(center, radius, color2);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_filled_circle(const ff::point_float& center, float radius, int inside_color, int outside_color)
{
    DirectX::XMFLOAT4 inside_color2, outside_color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(inside_color), inside_color2);
    ff::dxgi::palette_index_to_color(this->remap_palette_index(outside_color), outside_color2);
    this->draw_filled_circle(center, radius, inside_color2, outside_color2);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_outline_rectangle(const ff::rect_float& rect, int color, float thickness, bool pixel_thickness)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_outline_rectangle(rect, color2, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_outline_circle(const ff::point_float& center, float radius, int color, float thickness, bool pixel_thickness)
{
    DirectX::XMFLOAT4 color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(color), color2);
    this->draw_outline_circle(center, radius, color2, thickness, pixel_thickness);
}

void ff::dxgi::draw_util::draw_device_base::draw_palette_outline_circle(const ff::point_float& center, float radius, int inside_color, int outside_color, float thickness, bool pixel_thickness)
{
    DirectX::XMFLOAT4 inside_color2, outside_color2;
    ff::dxgi::palette_index_to_color(this->remap_palette_index(inside_color), inside_color2);
    ff::dxgi::palette_index_to_color(this->remap_palette_index(outside_color), outside_color2);
    this->draw_outline_circle(center, radius, inside_color2, outside_color2, thickness, pixel_thickness);
}

ff::dxgi::matrix_stack& ff::dxgi::draw_util::draw_device_base::world_matrix_stack()
{
    return this->world_matrix_stack_;
}

void ff::dxgi::draw_util::draw_device_base::nudge_depth()
{
    this->last_depth_type = ff::dxgi::draw_util::last_depth_type::nudged;
}

void ff::dxgi::draw_util::draw_device_base::push_palette(ff::dxgi::palette_base* palette)
{
    assert(!this->target_requires_palette && palette);
    this->palette_stack.push_back(palette);
    this->palette_index = ff::constants::invalid_dword;

    this->push_palette_remap(palette->index_remap(), palette->index_remap_hash());
}

void ff::dxgi::draw_util::draw_device_base::pop_palette()
{
    assert(this->palette_stack.size() > 1);
    this->palette_stack.pop_back();
    this->palette_index = ff::constants::invalid_dword;

    this->pop_palette_remap();
}

void ff::dxgi::draw_util::draw_device_base::push_palette_remap(const uint8_t* remap, size_t hash)
{
    this->palette_remap_stack.push_back(std::make_pair(
        remap ? remap : ff::dxgi::draw_util::default_palette_remap().data(),
        remap ? (hash ? hash : ff::stable_hash_bytes(remap, ff::dxgi::palette_size)) : ff::dxgi::draw_util::default_palette_remap_hash()));
    this->palette_remap_index = ff::constants::invalid_dword;
}

void ff::dxgi::draw_util::draw_device_base::pop_palette_remap()
{
    assert(this->palette_remap_stack.size() > 1);
    this->palette_remap_stack.pop_back();
    this->palette_remap_index = ff::constants::invalid_dword;
}

void ff::dxgi::draw_util::draw_device_base::push_no_overlap()
{
    this->force_no_overlap++;
}

void ff::dxgi::draw_util::draw_device_base::pop_no_overlap()
{
    assert(this->force_no_overlap > 0);

    if (!--this->force_no_overlap)
    {
        this->nudge_depth();
    }
}

void ff::dxgi::draw_util::draw_device_base::push_opaque()
{
    this->force_opaque++;
}

void ff::dxgi::draw_util::draw_device_base::pop_opaque()
{
    assert(this->force_opaque > 0);
    this->force_opaque--;
}

void ff::dxgi::draw_util::draw_device_base::push_pre_multiplied_alpha()
{
    if (!this->force_pre_multiplied_alpha)
    {
        this->flush();
    }

    this->force_pre_multiplied_alpha++;
}

void ff::dxgi::draw_util::draw_device_base::pop_pre_multiplied_alpha()
{
    assert(this->force_pre_multiplied_alpha > 0);

    if (this->force_pre_multiplied_alpha == 1)
    {
        this->flush();
    }

    this->force_pre_multiplied_alpha--;
}

void ff::dxgi::draw_util::draw_device_base::push_custom_context(ff::dxgi::draw_base::custom_context_func&& func)
{
    this->flush();
    this->custom_context_stack.push_back(std::move(func));
}

void ff::dxgi::draw_util::draw_device_base::pop_custom_context()
{
    assert(this->custom_context_stack.size());

    this->flush();
    this->custom_context_stack.pop_back();
}

void ff::dxgi::draw_util::draw_device_base::push_sampler_linear_filter(bool linear_filter)
{
    this->flush();
    this->sampler_stack.push_back(linear_filter);
}

void ff::dxgi::draw_util::draw_device_base::pop_sampler_linear_filter()
{
    assert(this->sampler_stack.size() > 1);

    this->flush();
    this->sampler_stack.pop_back();
}

ff::dxgi::device_child_base* ff::dxgi::draw_util::draw_device_base::as_device_child()
{
    return this;
}

bool ff::dxgi::draw_util::draw_device_base::internal_valid() const
{
    return this->state != draw_device_base::state_t::invalid;
}

ff::dxgi::draw_ptr ff::dxgi::draw_util::draw_device_base::internal_begin_draw(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect, const ff::rect_float& world_rect, ff::dxgi::draw_options options)
{
    this->end_draw();

    if (ff::dxgi::draw_util::setup_view_matrix(target, view_rect, world_rect, this->view_matrix) &&
        (this->command_context_ = this->setup(target, depth, view_rect)) != nullptr)
    {
        this->init_geometry_constant_buffers_0(target, view_rect, world_rect);
        this->target_requires_palette = ff::dxgi::palette_format(target.format());
        this->force_pre_multiplied_alpha = ff::flags::has(options, ff::dxgi::draw_options::pre_multiplied_alpha) && ff::dxgi::supports_pre_multiplied_alpha(target.format()) ? 1 : 0;
        this->state = draw_device_base::state_t::drawing;

        return this;
    }

    return nullptr;
}

bool ff::dxgi::draw_util::draw_device_base::reset()
{
    this->destroy();

    // Geometry buckets
    for (auto& i : this->geometry_buckets)
    {
        i.reset();
    }

    // Palette
    this->palette_stack.push_back(nullptr);
    this->palette_texture = this->create_texture(ff::point_size(ff::dxgi::palette_size, ff::dxgi::draw_util::MAX_PALETTES), ff::dxgi::PALETTE_FORMAT);

    this->palette_remap_stack.push_back(std::make_pair(ff::dxgi::draw_util::default_palette_remap().data(), ff::dxgi::draw_util::default_palette_remap_hash()));
    this->palette_remap_texture = this->create_texture(ff::point_size(ff::dxgi::palette_size, ff::dxgi::draw_util::MAX_PALETTE_REMAPS), ff::dxgi::PALETTE_INDEX_FORMAT);

    // States
    this->sampler_stack.push_back(false);

    this->internal_reset();

    this->state = draw_device_base::state_t::valid;
    return true;
}

void ff::dxgi::draw_util::draw_device_base::destroy()
{
    this->state = draw_device_base::state_t::invalid;

    this->internal_destroy();

    this->geometry_constants_0 = ff::dxgi::draw_util::geometry_shader_constants_0{};
    this->geometry_constants_1 = ff::dxgi::draw_util::geometry_shader_constants_1{};
    this->pixel_constants_0 = ff::dxgi::draw_util::pixel_shader_constants_0{};
    this->geometry_constants_hash_0 = 0;
    this->geometry_constants_hash_1 = 0;
    this->pixel_constants_hash_0 = 0;

    this->sampler_stack.clear();
    this->custom_context_stack.clear();

    this->view_matrix = ff::dxgi::matrix_identity_4x4();
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
    this->last_depth_type = ff::dxgi::draw_util::last_depth_type::none;
    this->draw_depth = 0;
    this->force_no_overlap = 0;
    this->force_opaque = 0;
    this->force_pre_multiplied_alpha = 0;

    for (auto& bucket : this->geometry_buckets)
    {
        bucket.reset();
    }
}

void ff::dxgi::draw_util::draw_device_base::flush(bool end_draw)
{
    if (this->last_depth_type != ff::dxgi::draw_util::last_depth_type::none && this->create_geometry_buffer())
    {
        this->update_geometry_constant_buffers_0();
        this->update_geometry_constant_buffers_1();
        this->update_pixel_constant_buffers_0();

        if (this->target_requires_palette || this->textures_using_palette_count)
        {
            this->update_palette_texture(*this->command_context_,
                this->target_requires_palette, this->textures_using_palette_count,
                *this->palette_texture, this->palette_texture_hashes.data(), this->palette_to_index,
                *this->palette_remap_texture, this->palette_remap_texture_hashes.data(), this->palette_remap_to_index);
        }

        this->apply_shader_input(*this->command_context_,
            this->target_requires_palette, this->sampler_stack.back(),
            this->texture_count, this->textures.data(),
            this->textures_using_palette_count, this->textures_using_palette.data(),
            *this->palette_texture, *this->palette_remap_texture);

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
        this->last_depth_type = ff::dxgi::draw_util::last_depth_type::none;

        this->command_context_ = this->internal_flush(*this->command_context_, end_draw);
    }
}

ff::dxgi::command_context_base* ff::dxgi::draw_util::draw_device_base::setup(ff::dxgi::target_base& target, ff::dxgi::depth_base* depth, const ff::rect_float& view_rect)
{
    if (depth && !depth->size(target.size().pixel_size))
    {
        return nullptr;
    }

    ff::dxgi::command_context_base* context = this->internal_setup(target, depth, view_rect);

    if (context && depth)
    {
        depth->clear(*context, 0, 0);
    }

    return context;
}

void ff::dxgi::draw_util::draw_device_base::matrix_changing(const ff::dxgi::matrix_stack& matrix_stack)
{
    this->world_matrix_index = ff::constants::invalid_dword;
}

void ff::dxgi::draw_util::draw_device_base::draw_line_strip(const ff::point_float* points, size_t point_count, const DirectX::XMFLOAT4* colors, size_t color_count, float thickness, bool pixel_thickness)
{
    assert(color_count == 1 || color_count == point_count);
    thickness = pixel_thickness ? -std::abs(thickness) : std::abs(thickness);

    ff::dxgi::vertex::line_geometry input;
    input.matrix_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index() : this->world_matrix_index;
    input.depth = this->nudge_depth(this->force_no_overlap ? ff::dxgi::draw_util::last_depth_type::line_no_overlap : ff::dxgi::draw_util::last_depth_type::line);
    input.color[0] = colors[0];
    input.color[1] = colors[0];
    input.thickness[0] = thickness;
    input.thickness[1] = thickness;

    const DirectX::XMFLOAT2* dxpoints = reinterpret_cast<const DirectX::XMFLOAT2*>(points);
    bool closed = point_count > 2 && points[0] == points[point_count - 1];
    ff::dxgi::draw_util::alpha_type alpha_type = ff::dxgi::draw_util::get_alpha_type(colors[0], this->force_opaque || this->target_requires_palette);

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
            alpha_type = ff::dxgi::draw_util::get_alpha_type(colors + i, 2, this->force_opaque || this->target_requires_palette);
        }

        if (alpha_type != ff::dxgi::draw_util::alpha_type::invisible)
        {
            ff::dxgi::draw_util::geometry_bucket_type bucket_type = (alpha_type == ff::dxgi::draw_util::alpha_type::transparent) ? ff::dxgi::draw_util::geometry_bucket_type::lines_alpha : ff::dxgi::draw_util::geometry_bucket_type::lines;
            this->add_geometry(&input, bucket_type, input.depth);
        }
    }
}

void ff::dxgi::draw_util::draw_device_base::init_geometry_constant_buffers_0(ff::dxgi::target_base& target, const ff::rect_float& view_rect, const ff::rect_float& world_rect)
{
    this->geometry_constants_0.view_size = view_rect.size() / static_cast<float>(target.size().dpi_scale);
    this->geometry_constants_0.view_scale = world_rect.size() / this->geometry_constants_0.view_size;
}

void ff::dxgi::draw_util::draw_device_base::update_geometry_constant_buffers_0()
{
    this->geometry_constants_0.projection = this->view_matrix;

    size_t hash0 = ff::stable_hash_func(this->geometry_constants_0);
    if (!this->geometry_constants_hash_0 || this->geometry_constants_hash_0 != hash0)
    {
        this->geometry_constants_buffer_0().update(*this->command_context_, &this->geometry_constants_0, sizeof(ff::dxgi::draw_util::geometry_shader_constants_0));
        this->geometry_constants_hash_0 = hash0;
    }
}

void ff::dxgi::draw_util::draw_device_base::update_geometry_constant_buffers_1()
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
        size_t buffer_size = sizeof(DirectX::XMFLOAT4X4) * ff::dxgi::draw_util::MAX_TRANSFORM_MATRIXES;
#else
        size_t buffer_size = sizeof(DirectX::XMFLOAT4X4) * world_matrix_count;
#endif
        this->geometry_constants_buffer_1().update(*this->command_context_, this->geometry_constants_1.model.data(), ff::vector_byte_size(this->geometry_constants_1.model), buffer_size);
    }
}

void ff::dxgi::draw_util::draw_device_base::update_pixel_constant_buffers_0()
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
            this->pixel_constants_buffer_0().update(*this->command_context_, &this->pixel_constants_0, sizeof(ff::dxgi::draw_util::pixel_shader_constants_0));
            this->pixel_constants_hash_0 = hash0;
        }
    }
}

bool ff::dxgi::draw_util::draw_device_base::create_geometry_buffer()
{
    size_t byte_size = 0;

    for (ff::dxgi::draw_util::geometry_bucket& bucket : this->geometry_buckets)
    {
        byte_size = ff::math::round_up(byte_size, bucket.item_size());
        bucket.render_start(byte_size / bucket.item_size());
        byte_size += bucket.byte_size();
    }

    void* buffer_data = this->geometry_buffer().map(*this->command_context_, byte_size);
    if (buffer_data)
    {
        for (ff::dxgi::draw_util::geometry_bucket& bucket : this->geometry_buckets)
        {
            if (bucket.render_count())
            {
                ::memcpy(reinterpret_cast<uint8_t*>(buffer_data) + bucket.render_start() * bucket.item_size(), bucket.data(), bucket.byte_size());
                bucket.clear_items();
            }
        }

        this->geometry_buffer().unmap();

        return true;
    }

    assert(false);
    return false;
}

void ff::dxgi::draw_util::draw_device_base::draw_opaque_geometry()
{
    const ff::dxgi::draw_base::custom_context_func* custom_func = this->custom_context_stack.size() ? &this->custom_context_stack.back() : nullptr;
    this->apply_opaque_state(*this->command_context_);

    for (ff::dxgi::draw_util::geometry_bucket& bucket : this->geometry_buckets)
    {
        if (bucket.bucket_type() >= ff::dxgi::draw_util::geometry_bucket_type::first_alpha)
        {
            break;
        }

        if (bucket.render_count())
        {
            this->apply_geometry_buffer(*this->command_context_, bucket.bucket_type(), this->geometry_buffer(), this->target_requires_palette);

            if (!custom_func || (*custom_func)(bucket.item_type(), true))
            {
                this->draw(*this->command_context_, bucket.render_count(), bucket.render_start());
            }
        }
    }
}

void ff::dxgi::draw_util::draw_device_base::draw_alpha_geometry()
{
    const size_t alpha_geometry_size = this->alpha_geometry.size();
    if (alpha_geometry_size)
    {
        const ff::dxgi::draw_base::custom_context_func* custom_func = this->custom_context_stack.size() ? &this->custom_context_stack.back() : nullptr;
        this->apply_alpha_state(*this->command_context_, this->force_pre_multiplied_alpha);

        for (size_t i = 0; i < alpha_geometry_size; )
        {
            const ff::dxgi::draw_util::alpha_geometry_entry& entry = this->alpha_geometry[i];
            size_t geometry_count = 1;

            for (i++; i < alpha_geometry_size; i++, geometry_count++)
            {
                const ff::dxgi::draw_util::alpha_geometry_entry& entry2 = this->alpha_geometry[i];
                if (entry2.bucket != entry.bucket ||
                    entry2.depth != entry.depth ||
                    entry2.index != entry.index + geometry_count)
                {
                    break;
                }
            }

            this->apply_geometry_buffer(*this->command_context_, entry.bucket->bucket_type(), this->geometry_buffer(), this->target_requires_palette);

            if (!custom_func || (*custom_func)(entry.bucket->item_type(), false))
            {
                this->draw(*this->command_context_, geometry_count, entry.bucket->render_start() + entry.index);
            }
        }
    }
}

float ff::dxgi::draw_util::draw_device_base::nudge_depth(ff::dxgi::draw_util::last_depth_type depth_type)
{
    if (depth_type < ff::dxgi::draw_util::last_depth_type::start_no_overlap || this->last_depth_type != depth_type)
    {
        this->draw_depth += ff::dxgi::draw_util::RENDER_DEPTH_DELTA;
    }

    this->last_depth_type = depth_type;
    return this->draw_depth;
}

unsigned int ff::dxgi::draw_util::draw_device_base::get_world_matrix_index()
{
    unsigned int index = this->get_world_matrix_index_no_flush();
    if (index == ff::constants::invalid_dword)
    {
        this->flush();
        index = this->get_world_matrix_index_no_flush();
    }

    return index;
}

unsigned int ff::dxgi::draw_util::draw_device_base::get_world_matrix_index_no_flush()
{
    if (this->world_matrix_index == ff::constants::invalid_dword)
    {
        DirectX::XMFLOAT4X4 wm;
        DirectX::XMStoreFloat4x4(&wm, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&this->world_matrix_stack_.matrix())));
        auto iter = this->world_matrix_to_index.find(wm);

        if (iter == this->world_matrix_to_index.cend() && this->world_matrix_to_index.size() != ::ff::dxgi::draw_util::MAX_TRANSFORM_MATRIXES)
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

unsigned int ff::dxgi::draw_util::draw_device_base::get_texture_index_no_flush(const ff::dxgi::texture_view_base& texture_view, bool use_palette)
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
            if (this->textures_using_palette_count == ff::dxgi::draw_util::MAX_TEXTURES_USING_PALETTE)
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
            if (this->texture_count == ff::dxgi::draw_util::MAX_TEXTURES)
            {
                return ff::constants::invalid_dword;
            }

            this->textures[this->texture_count] = &texture_view;
            texture_index = static_cast<unsigned int>(this->texture_count++);
        }

        return texture_index | (palette_remap_index << 16);
    }
}

unsigned int ff::dxgi::draw_util::draw_device_base::get_palette_index_no_flush()
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
            ff::dxgi::palette_base* palette = this->palette_stack.back();
            size_t palette_hash = palette ? palette->data()->row_hash(palette->current_row()) : 0;
            auto iter = this->palette_to_index.find(palette_hash);

            if (iter == this->palette_to_index.cend() && this->palette_to_index.size() != ff::dxgi::draw_util::MAX_PALETTES)
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

unsigned int ff::dxgi::draw_util::draw_device_base::get_palette_remap_index_no_flush()
{
    if (this->palette_remap_index == ff::constants::invalid_dword)
    {
        auto& remap_pair = this->palette_remap_stack.back();
        auto iter = this->palette_remap_to_index.find(remap_pair.second);

        if (iter == this->palette_remap_to_index.cend() && this->palette_remap_to_index.size() != ff::dxgi::draw_util::MAX_PALETTE_REMAPS)
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

int ff::dxgi::draw_util::draw_device_base::remap_palette_index(int color) const
{
    return this->palette_remap_stack.back().first[color];
}

void ff::dxgi::draw_util::draw_device_base::get_world_matrix_and_texture_index(const ff::dxgi::texture_view_base& texture_view, bool use_palette, unsigned int& model_index, unsigned int& texture_index)
{
    model_index = (this->world_matrix_index == ff::constants::invalid_dword) ? this->get_world_matrix_index_no_flush() : this->world_matrix_index;
    texture_index = this->get_texture_index_no_flush(texture_view, use_palette);

    if (model_index == ff::constants::invalid_dword || texture_index == ff::constants::invalid_dword)
    {
        this->flush();
        this->get_world_matrix_and_texture_index(texture_view, use_palette, model_index, texture_index);
    }
}

void ff::dxgi::draw_util::draw_device_base::get_world_matrix_and_texture_indexes(ff::dxgi::texture_view_base* const* texture_views, bool use_palette, unsigned int* texture_indexes, size_t count, unsigned int& model_index)
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

void* ff::dxgi::draw_util::draw_device_base::add_geometry(const void* data, ff::dxgi::draw_util::geometry_bucket_type bucket_type, float depth)
{
    ff::dxgi::draw_util::geometry_bucket& bucket = this->get_geometry_bucket(bucket_type);

    if (bucket_type >= ff::dxgi::draw_util::geometry_bucket_type::first_alpha)
    {
        assert(!this->force_opaque);

        this->alpha_geometry.push_back(ff::dxgi::draw_util::alpha_geometry_entry
            {
            &bucket,
            bucket.count(),
            depth
            });
    }

    return bucket.add(data);
}

ff::dxgi::draw_util::geometry_bucket& ff::dxgi::draw_util::draw_device_base::get_geometry_bucket(ff::dxgi::draw_util::geometry_bucket_type type)
{
    return this->geometry_buckets[static_cast<size_t>(type)];
}
