#include "pch.h"
#include "draw_util.h"
#include "sprite_data.h"
#include "sprite_type.h"
#include "target_base.h"

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
    : vs_res_name(std::move(rhs.vs_res_name))
    , gs_res_name(std::move(rhs.gs_res_name))
    , ps_res_name(std::move(rhs.ps_res_name))
    , ps_palette_out_res_name(std::move(rhs.ps_palette_out_res_name))
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

ff::dxgi::draw_util::geometry_bucket::~geometry_bucket()
{
    ::_aligned_free(this->data_start);
}

void ff::dxgi::draw_util::geometry_bucket::reset(std::string_view vs_res, std::string_view gs_res, std::string_view ps_res, std::string_view ps_palette_out_res)
{
    this->reset();

    this->vs_res_name = vs_res;
    this->gs_res_name = gs_res;
    this->ps_res_name = ps_res;
    this->ps_palette_out_res_name = ps_palette_out_res;
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
