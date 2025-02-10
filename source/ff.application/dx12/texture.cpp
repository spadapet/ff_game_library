#include "pch.h"
#include "dx12/commands.h"
#include "dx12/descriptor_allocator.h"
#include "dx12/device_reset_priority.h"
#include "dx12/dx12_globals.h"
#include "dx12/resource.h"
#include "dx12/texture.h"
#include "dx_types/color.h"
#include "dxgi/format_util.h"

static std::atomic_int dynamic_texture_counter;
static std::atomic_int static_texture_counter;

ff::dx12::texture::texture()
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(ff::point_size size, DXGI_FORMAT format, size_t mip_count, size_t array_size, size_t sample_count, const ff::color* optimized_clear_color)
{
    format = ff::dxgi::fix_format(format, static_cast<size_t>(size.x), static_cast<size_t>(size.y), mip_count);

    if (!ff::dxgi::compressed_format(format) && size.x > 0 && size.y > 0 && mip_count > 0 && array_size > 0 && sample_count > 0)
    {
        D3D12_CLEAR_VALUE clear_value{};

        if (optimized_clear_color)
        {
            const DirectX::XMFLOAT4 optimized_clear_color2 = optimized_clear_color->to_shader_color();
            clear_value.Format = format;
            std::memcpy(clear_value.Color, &optimized_clear_color2.x, sizeof(clear_value.Color));
        }

        this->resource_ = std::make_unique<ff::dx12::resource>(
            ff::string::concat("Dynamic texture ", ::dynamic_texture_counter.fetch_add(1)),
            CD3DX12_RESOURCE_DESC::Tex2D(format,
                static_cast<UINT64>(size.x),
                static_cast<UINT>(size.y),
                static_cast<UINT16>(array_size),
                static_cast<UINT16>(mip_count),
                static_cast<UINT>(ff::dx12::fix_sample_count(format, sample_count)), 0, // quality
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            clear_value);
    }
    else
    {
        assert(false);
    }

    this->sprite_type_ = ff::dxgi::palette_format(format)
        ? ff::dxgi::sprite_type::opaque_palette
        : (ff::dxgi::has_alpha(format) ? ff::dxgi::sprite_type::transparent : ff::dxgi::sprite_type::opaque);

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(const std::shared_ptr<DirectX::ScratchImage>& data, ff::dxgi::sprite_type sprite_type)
    : data_(data)
    , upload_data_pending(true)
{
    const DirectX::TexMetadata& md = this->data_->GetMetadata();

    this->resource_ = std::make_unique<ff::dx12::resource>(
        ff::string::concat("Static texture ", ::static_texture_counter.fetch_add(1)),
        std::shared_ptr<ff::dx12::mem_range>(), // allocate new memory
        CD3DX12_RESOURCE_DESC::Tex2D(md.format,
            static_cast<UINT64>(md.width),
            static_cast<UINT>(md.height),
            static_cast<UINT16>(md.arraySize),
            static_cast<UINT16>(md.mipLevels)));

    this->sprite_type_ = (sprite_type == ff::dxgi::sprite_type::unknown && this->data_)
        ? ff::dxgi::get_sprite_type(*this->data_)
        : sprite_type;

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::texture(texture&& other) noexcept
    : resource_(std::move(other.resource_))
    , data_(std::move(other.data_))
    , view_(std::move(other.view_))
    , sprite_type_(std::move(other.sprite_type_))
    , upload_data_pending(std::move(other.upload_data_pending))
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::normal);
}

ff::dx12::texture::~texture()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::texture& ff::dx12::texture::get(ff::dxgi::texture_base& other)
{
    return static_cast<ff::dx12::texture&>(other);
}

ff::dx12::texture& ff::dx12::texture::operator=(texture&& other) noexcept
{
    return this->assign(std::move(other));
}

ff::dx12::texture& ff::dx12::texture::assign(texture&& other) noexcept
{
    if (this != &other)
    {
        this->resource_ = std::move(other.resource_);
        this->view_ = std::move(other.view_);
        this->data_ = std::move(other.data_);
        this->sprite_type_ = std::move(other.sprite_type_);
        this->upload_data_pending = std::move(other.upload_data_pending);

        this->on_reset();
    }

    return *this;
}

void ff::dx12::texture::on_reset()
{
    // override this
}

ff::point_size ff::dx12::texture::size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return ff::point_size(md.width, md.height);
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return ff::point_t<uint64_t>(desc.Width, desc.Height).cast<size_t>();
    }

    assert(false);
    return {};
}

size_t ff::dx12::texture::mip_count() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.mipLevels;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.MipLevels);
    }

    assert(false);
    return 0;
}

size_t ff::dx12::texture::array_size() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.arraySize;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.DepthOrArraySize);
    }

    assert(false);
    return 0;
}

size_t ff::dx12::texture::sample_count() const
{
    if (this->data_)
    {
        return 1;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return static_cast<size_t>(desc.SampleDesc.Count);
    }

    assert(false);
    return 1;
}

DXGI_FORMAT ff::dx12::texture::format() const
{
    if (this->data_)
    {
        const DirectX::TexMetadata& md = this->data_->GetMetadata();
        return md.format;
    }

    if (this->resource_)
    {
        const D3D12_RESOURCE_DESC& desc = this->resource_->desc();
        return desc.Format;
    }

    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}

ff::dx12::texture::operator bool() const
{
    return this->resource_ || (this->data_ && this->data_->GetImageCount());
}

ff::dxgi::sprite_type ff::dx12::texture::sprite_type() const
{
    return this->sprite_type_;
}

std::shared_ptr<DirectX::ScratchImage> ff::dx12::texture::data() const
{
    if (!this->data_ && this->resource_)
    {
        DirectX::ScratchImage scratch;
        ff::dx12::resource* resource = this->resource_.get();

        // Can't use the device context on background threads
        if (ff::thread_dispatch::get_game()->send([&scratch, resource]()
            {
                size_t image_count = resource->desc().MipLevels * resource->desc().DepthOrArraySize;
                scratch = resource->capture_texture(nullptr, 0, image_count, nullptr);
            }, 500))
        {
            return scratch.GetImageCount() ? std::make_shared<DirectX::ScratchImage>(std::move(scratch)) : nullptr;
        }
    }

    return this->data_;
}

ff::dx12::resource* ff::dx12::texture::dx12_resource_updated(ff::dx12::commands& commands)
{
    if (this->upload_data_pending)
    {
        this->upload_data_pending = false;
        this->resource_->update_texture(&commands, this->data_->GetImages(), 0, this->data_->GetImageCount(), ff::point_size{});
    }

    return this->resource_.get();
}

ff::dx12::resource* ff::dx12::texture::dx12_resource() const
{
    return this->resource_.get();
}

D3D12_CLEAR_VALUE ff::dx12::texture::optimized_clear_value() const
{
    return this->resource_ ? this->resource_->optimized_clear_value() : D3D12_CLEAR_VALUE{};
}

bool ff::dx12::texture::update(ff::dxgi::command_context_base& context, size_t array_index, size_t mip_index, const ff::point_size& pos, const DirectX::Image& data)
{
    if (this->format() != data.format)
    {
        debug_fail_msg("Wrong format for texture update");
        return false;
    }

    if (this->data_ && FAILED(DirectX::CopyRectangle(data,
        DirectX::Rect(0, 0, data.width, data.height),
        *this->data_->GetImage(mip_index, array_index, 0),
        DirectX::TEX_FILTER_DEFAULT,
        static_cast<size_t>(pos.x),
        static_cast<size_t>(pos.y))))
    {
        debug_fail_msg("Failed to update texture scratch image");
    }

    if (!this->upload_data_pending)
    {
        UINT sub_index = ::D3D12CalcSubresource(static_cast<UINT>(mip_index), static_cast<UINT>(array_index), 0, static_cast<UINT>(this->mip_count()), static_cast<UINT>(this->array_size()));
        ff::dx12::commands& commands = ff::dx12::commands::get(context);
        this->resource_->update_texture(&commands, &data, sub_index, 1, pos.cast<size_t>());
    }

    return true;
}

bool ff::dx12::texture::reset()
{
    if (this->data_)
    {
        *this = ff::dx12::texture(this->data_, this->sprite_type());
    }
    else
    {
        const D3D12_CLEAR_VALUE clear_value = this->optimized_clear_value();
        const ff::color* clear_color{};
        ff::color clear_color_storage;
        if (clear_value.Format != DXGI_FORMAT_UNKNOWN)
        {
            clear_color_storage = ff::color(clear_value.Color[0], clear_value.Color[1], clear_value.Color[2], clear_value.Color[3]);
            clear_color = &clear_color_storage;
        }
        
        *this = ff::dx12::texture(this->size(), this->format(), this->mip_count(), this->array_size(), this->sample_count(), clear_color);
    }

    return *this;
}

ff::dxgi::texture_view_access_base& ff::dx12::texture::view_access()
{
    return *this;
}

ff::dxgi::texture_base* ff::dx12::texture::view_texture()
{
    return this;
}

size_t ff::dx12::texture::view_array_start() const
{
    return 0;
}

size_t ff::dx12::texture::view_array_size() const
{
    return this->array_size();
}

size_t ff::dx12::texture::view_mip_start() const
{
    return 0;
}

size_t ff::dx12::texture::view_mip_size() const
{
    return this->mip_count();
}

D3D12_CPU_DESCRIPTOR_HANDLE ff::dx12::texture::dx12_texture_view() const
{
    if (!this->view_)
    {
        this->view_ = ff::dx12::cpu_buffer_descriptors().alloc_range(1);
        this->dx12_resource()->create_shader_view(this->view_.cpu_handle(0));
    }

    return this->view_.cpu_handle(0);
}
