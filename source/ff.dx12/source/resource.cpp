#include "pch.h"
#include "commands.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "mem_allocator.h"
#include "mem_range.h"
#include "queue.h"
#include "resource.h"
#include "resource_tracker.h"

static std::unique_ptr<ff::dx12::commands> get_copy_commands(ff::dx12::commands*& commands)
{
    std::unique_ptr<ff::dx12::commands> new_commands;
    if (!commands)
    {
        new_commands = std::make_unique<ff::dx12::commands>(ff::dx12::copy_queue().new_commands());
        commands = new_commands.get();
    }

    return new_commands;
}

static bool validate_texture_range(const D3D12_RESOURCE_DESC& desc, size_t sub_index, size_t sub_count, const ff::rect_size* source_rect, size_t& width, size_t& height, size_t& mip_count, size_t& array_count)
{
    const size_t mip_start = sub_index % desc.MipLevels;

    width = source_rect ? source_rect->width() : static_cast<size_t>(desc.Width >> mip_start);
    height = source_rect ? source_rect->height() : static_cast<size_t>(desc.Height >> mip_start);
    mip_count = std::min<size_t>(sub_count, desc.MipLevels);
    array_count = std::max<size_t>(sub_count / desc.MipLevels, 1);

    // For multiple images in an array, all of the mip levels must be captured too
    return sub_count && (mip_start + sub_count <= desc.MipLevels || (!mip_start && sub_count % desc.MipLevels == 0));
}

size_t ff::dx12::resource::readback_texture_data::image_count() const
{
    return this->mem_ranges.size();
}

const DirectX::Image& ff::dx12::resource::readback_texture_data::image(size_t index)
{
    this->fence_value.wait(nullptr);
    return this->mem_ranges[index].second;
}

ff::dx12::resource::resource(std::shared_ptr<ff::dx12::mem_range> mem_range, const D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE optimized_clear_value)
    : resource(desc, D3D12_RESOURCE_STATE_COMMON, optimized_clear_value, mem_range, true)
{}

ff::dx12::resource::resource(const D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE optimized_clear_value)
    : resource(desc, D3D12_RESOURCE_STATE_COMMON, optimized_clear_value, {}, false)
{}

ff::dx12::resource::resource(
    const D3D12_RESOURCE_DESC& desc,
    D3D12_RESOURCE_STATES initial_state,
    D3D12_CLEAR_VALUE optimized_clear_value,
    std::shared_ptr<ff::dx12::mem_range> mem_range,
    bool allocate_mem_range)
    : desc_(desc)
    , alloc_info_(ff::dx12::device()->GetResourceAllocationInfo(0, 1, &desc))
    , optimized_clear_value(optimized_clear_value)
    , mem_range_(mem_range)
    , global_state_(initial_state, ff::dx12::resource_state::type_t::global, static_cast<size_t>(desc.DepthOrArraySize), static_cast<size_t>(desc.MipLevels))
{
    assert(desc.Dimension != D3D12_RESOURCE_DIMENSION_UNKNOWN && this->alloc_info_.SizeInBytes > 0 && desc.MipLevels * desc.DepthOrArraySize > 0);

    if (allocate_mem_range)
    {
        if (!mem_range || ff::math::align_up(mem_range->start(), this->alloc_info_.Alignment) != mem_range->start() || mem_range->size() < this->alloc_info_.SizeInBytes)
        {
            ff::dx12::mem_allocator& allocator = (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
                ? ff::dx12::static_buffer_allocator()
                : ff::dx12::texture_allocator();
            this->mem_range_ = std::make_shared<ff::dx12::mem_range>(
                allocator.alloc_bytes(this->alloc_info_.SizeInBytes, this->alloc_info_.Alignment));
        }
    }
    else
    {
        assert(!mem_range);
    }

    this->reset();
    assert(*this && (!mem_range || mem_range == this->mem_range_));

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::resource);
}

ff::dx12::resource::resource(resource& other, ff::dx12::commands* commands)
    : resource(other.desc_, other.optimized_clear_value)
{
    std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);
    commands->copy_resource(*this, other);
}

ff::dx12::resource::resource(resource&& other) noexcept
{
    *this = std::move(other);
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::resource);
}

ff::dx12::resource::~resource()
{
    this->destroy(false);
    ff::dx12::remove_device_child(this);
}

ff::dx12::resource& ff::dx12::resource::operator=(resource&& other) noexcept
{
    if (this != &other)
    {
        this->destroy(false);

        std::swap(this->resource_, other.resource_);
        std::swap(this->mem_range_, other.mem_range_);
        std::swap(this->optimized_clear_value, other.optimized_clear_value);
        std::swap(this->desc_, other.desc_);
        std::swap(this->alloc_info_, other.alloc_info_);
        std::swap(this->global_state_, other.global_state_);
        std::swap(this->global_reads_, other.global_reads_);
        std::swap(this->global_write_, other.global_write_);
    }

    return *this;
}

ff::dx12::resource::operator bool() const
{
    return this->resource_ != nullptr;
}

const std::shared_ptr<ff::dx12::mem_range>& ff::dx12::resource::mem_range() const
{
    return this->mem_range_;
}

const D3D12_RESOURCE_DESC& ff::dx12::resource::desc() const
{
    return this->desc_;
}

const D3D12_RESOURCE_ALLOCATION_INFO& ff::dx12::resource::alloc_info() const
{
    return this->alloc_info_;
}

size_t ff::dx12::resource::sub_resource_size() const
{
    return this->array_size() * this->mip_size();
}

size_t ff::dx12::resource::array_size() const
{
    return static_cast<size_t>(this->desc_.DepthOrArraySize);
}

size_t ff::dx12::resource::mip_size() const
{
    return static_cast<size_t>(this->desc_.MipLevels);
}

ff::dx12::resource_state& ff::dx12::resource::global_state()
{
    return this->global_state_;
}

void ff::dx12::resource::prepare_state(
    ff::dx12::fence_values& wait_before_execute,
    const ff::dx12::fence_value& next_fence_value,
    ff::dx12::resource_tracker& tracker,
    D3D12_RESOURCE_STATES state,
    size_t array_start, size_t array_size, size_t mip_start, size_t mip_size)
{
    const D3D12_RESOURCE_STATES write_states =
        D3D12_RESOURCE_STATE_RENDER_TARGET |
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
        D3D12_RESOURCE_STATE_DEPTH_WRITE |
        D3D12_RESOURCE_STATE_STREAM_OUT |
        D3D12_RESOURCE_STATE_COPY_DEST |
        D3D12_RESOURCE_STATE_RESOLVE_DEST |
        D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE |
        D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE;

    if ((state & write_states) != 0)
    {
        wait_before_execute.add(this->global_reads_, this->global_write_);
        this->global_reads_.clear();
        this->global_write_ = next_fence_value;

        if ((state & ~write_states) != 0)
        {
            this->global_reads_.add(next_fence_value);
        }
    }
    else
    {
        wait_before_execute.add(this->global_write_);
        this->global_reads_.clear();
        this->global_reads_.add(next_fence_value);
        this->global_write_ = {};
    }

    tracker.state(*this, state, array_start, array_size, mip_start, mip_size);
}

ff::dx12::fence_value ff::dx12::resource::update_buffer(ff::dx12::commands* commands, const void* data, uint64_t offset, uint64_t size)
{
    if (size && data && offset + size <= this->alloc_info_.SizeInBytes)
    {
        std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);
        ff::dx12::mem_range mem_range = ff::dx12::upload_allocator().alloc_buffer(size, commands->next_fence_value());

        if (mem_range.cpu_data())
        {
            ::memcpy(mem_range.cpu_data(), data, static_cast<size_t>(size));
            commands->update_buffer(*this, offset, mem_range);
            return commands->next_fence_value();
        }
    }

    assert(!size);
    return {};
}

std::pair<ff::dx12::fence_value, ff::dx12::mem_range> ff::dx12::resource::readback_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size)
{
    if (size && offset + size <= this->alloc_info_.SizeInBytes)
    {
        std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);
        ff::dx12::mem_range mem_range = ff::dx12::readback_allocator().alloc_buffer(size, commands->next_fence_value());

        if (mem_range.cpu_data())
        {
            commands->readback_buffer(mem_range, *this, offset);
            return std::make_pair(commands->next_fence_value(), std::move(mem_range));
        }
    }

    assert(!size);
    return {};
}

std::vector<uint8_t> ff::dx12::resource::capture_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size)
{
    auto [fence_value, result_mem_range] = this->readback_buffer(commands, offset, size);
    if (fence_value)
    {
        std::vector<uint8_t> result_bytes;
        result_bytes.resize(static_cast<size_t>(size));
        fence_value.wait(nullptr);

        std::memcpy(result_bytes.data(), result_mem_range.cpu_data(), static_cast<size_t>(size));
        return result_bytes;
    }

    return {};
}

ff::dx12::fence_value ff::dx12::resource::update_texture(ff::dx12::commands* commands, const DirectX::Image* images, size_t sub_index, size_t sub_count, ff::point_size dest_pos)
{
    std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);

    size_t temp_width, temp_height, temp_mip_count, temp_array_count;
    if (!::validate_texture_range(this->desc_, sub_index, sub_count, nullptr, temp_width, temp_height, temp_mip_count, temp_array_count))
    {
        assert(!sub_count);
        return {};
    }

    for (size_t i = 0; i < sub_count; i++)
    {
        const DirectX::Image& src_image = images[i];
        DirectX::Image dest_image = src_image;
        DirectX::ComputePitch(src_image.format, src_image.width, src_image.height, dest_image.rowPitch, dest_image.slicePitch);

        const size_t scan_lines = DirectX::ComputeScanlines(dest_image.format, dest_image.height);
        dest_image.rowPitch = ff::math::align_up<size_t>(dest_image.rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        dest_image.slicePitch = dest_image.rowPitch * scan_lines;

        ff::dx12::mem_range mem_range = ff::dx12::upload_allocator().alloc_texture(dest_image.slicePitch, commands->next_fence_value());
        dest_image.pixels = static_cast<uint8_t*>(mem_range.cpu_data());

        if (!mem_range.cpu_data() || FAILED(DirectX::CopyRectangle(src_image, DirectX::Rect(0, 0, src_image.width, src_image.height), dest_image, DirectX::TEX_FILTER_DEFAULT, 0, 0)))
        {
            assert(false);
            continue;
        }

        const size_t relative_mip_level = i % this->desc_.MipLevels;
        const ff::point_size pos(dest_pos.x >> relative_mip_level, dest_pos.y >> relative_mip_level);
        commands->update_texture(*this, sub_index + i, pos, mem_range, D3D12_SUBRESOURCE_FOOTPRINT
        {
            dest_image.format, static_cast<UINT>(dest_image.width), static_cast<UINT>(dest_image.height), 1, static_cast<UINT>(dest_image.rowPitch),
        });
    }

    return commands->next_fence_value();
}

ff::dx12::resource::readback_texture_data ff::dx12::resource::readback_texture(ff::dx12::commands* commands, size_t sub_index, size_t sub_count, const ff::rect_size* source_rect)
{
    readback_texture_data result{};
    result.mem_ranges.reserve(sub_count);

    if (!::validate_texture_range(this->desc_, sub_index, sub_count, source_rect, result.width, result.height, result.mip_count, result.array_count))
    {
        assert(!sub_count);
        return {};
    }

    std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);

    for (size_t i = 0; i < sub_count; i++)
    {
        const size_t relative_mip_level = i % this->desc_.MipLevels;
        const size_t absolute_mip_level = (sub_index + i) % this->desc_.MipLevels;
        const ff::rect_size rect = source_rect
            ? ff::rect_size(source_rect->left >> relative_mip_level, source_rect->top >> relative_mip_level, source_rect->right >> relative_mip_level, source_rect->bottom >> relative_mip_level)
            : ff::rect_size(0, 0, result.width >> absolute_mip_level, result.height >> absolute_mip_level);

        DirectX::Image image;
        image.format = this->desc_.Format;
        image.width = rect.width();
        image.height = rect.height();
        DirectX::ComputePitch(image.format, image.width, image.height, image.rowPitch, image.slicePitch);

        const size_t scan_lines = DirectX::ComputeScanlines(this->desc_.Format, image.height);
        image.rowPitch = ff::math::align_up<size_t>(image.rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
        image.slicePitch = image.rowPitch * scan_lines;

        ff::dx12::mem_range mem_range = ff::dx12::readback_allocator().alloc_texture(image.slicePitch, commands->next_fence_value());
        if (!mem_range.cpu_data())
        {
            assert(false);
            continue;
        }

        image.pixels = static_cast<uint8_t*>(mem_range.cpu_data());

        const D3D12_SUBRESOURCE_FOOTPRINT layout
        {
            image.format,
            static_cast<UINT>(image.width),
            static_cast<UINT>(image.height), 1, // depth
            static_cast<UINT>(image.rowPitch),
        };

        commands->readback_texture(mem_range, layout, *this, sub_index + i, rect);
        result.mem_ranges.push_back(std::make_pair(std::move(mem_range), std::move(image)));
    }

    result.fence_value = commands->next_fence_value();
    return result;
}

DirectX::ScratchImage ff::dx12::resource::capture_texture(ff::dx12::commands* commands, size_t sub_index, size_t sub_count, const ff::rect_size* source_rect)
{
    readback_texture_data result = this->readback_texture(commands, sub_index, sub_count, source_rect);
    DirectX::ScratchImage scratch;

    if (!result.fence_value || !result.image_count() || result.array_count * result.mip_count != result.image_count() ||
        FAILED(scratch.Initialize2D(this->desc_.Format, result.width, result.height, result.array_count, result.mip_count)))
    {
        assert(false);
        return {};
    }

    for (size_t i = 0; i < result.image_count(); i++)
    {
        const DirectX::Image& image = result.image(i);
        HRESULT hr = DirectX::CopyRectangle(image, DirectX::Rect(0, 0, image.width, image.height), scratch.GetImages()[i], DirectX::TEX_FILTER_DEFAULT, 0, 0);
        assert(SUCCEEDED(hr));
    }

    return scratch;
}

void ff::dx12::resource::destroy(bool for_reset)
{
    if (!for_reset)
    {
        ff::dx12::fence_values fence_values = std::move(this->global_reads_);
        fence_values.add(std::move(this->global_write_));

        ff::dx12::keep_alive_resource(std::move(*this), std::move(fence_values));

        this->mem_range_.reset();
    }

    this->global_reads_.clear();
    this->global_write_ = {};
    this->resource_.Reset();
}

void ff::dx12::resource::before_reset()
{
    this->destroy(true);
}

bool ff::dx12::resource::reset()
{
    if (!this->mem_range_)
    {
        if (FAILED(ff::dx12::device()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &this->desc_,
            this->global_state_.get(0).first,
            (this->optimized_clear_value.Format != DXGI_FORMAT_UNKNOWN) ? &this->optimized_clear_value : nullptr,
            IID_PPV_ARGS(&this->resource_))))
        {
            return false;
        }
    }
    else
    {
        if (FAILED(ff::dx12::device()->CreatePlacedResource(
            ff::dx12::get_heap(*this->mem_range_->heap()),
            this->mem_range_->start(),
            &this->desc_,
            this->global_state_.get(0).first,
            (this->optimized_clear_value.Format != DXGI_FORMAT_UNKNOWN) ? &this->optimized_clear_value : nullptr,
            IID_PPV_ARGS(&this->resource_))))
        {
            return false;
        }
    }

    return true;
}
