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
        new_commands = ff::dx12::copy_queue().new_commands();
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

ff::dx12::resource::resource(std::string_view name, std::shared_ptr<ff::dx12::mem_range> mem_range, const D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE optimized_clear_value)
    : resource(name, desc, D3D12_RESOURCE_STATE_COMMON, optimized_clear_value, mem_range, true)
{}

ff::dx12::resource::resource(std::string_view name, const D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE optimized_clear_value)
    : resource(name, desc, D3D12_RESOURCE_STATE_COMMON, optimized_clear_value, {}, false)
{}

ff::dx12::resource::resource(std::string_view name, ID3D12ResourceX* swap_chain_resource)
    : name_(name)
    , desc_(swap_chain_resource->GetDesc())
    , optimized_clear_value_{}
    , resource_(swap_chain_resource)
    , external_resource(true)
    , global_state_(D3D12_RESOURCE_STATE_COMMON, ff::dx12::resource_state::type_t::global, static_cast<size_t>(this->desc_.DepthOrArraySize), static_cast<size_t>(this->desc_.MipLevels))
    , tracker_(nullptr)
{
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::resource);
}

ff::dx12::resource::resource(
    std::string_view name,
    const D3D12_RESOURCE_DESC& desc,
    D3D12_RESOURCE_STATES initial_state,
    D3D12_CLEAR_VALUE optimized_clear_value,
    std::shared_ptr<ff::dx12::mem_range> mem_range,
    bool allocate_mem_range)
    : name_(name)
    , desc_(desc)
    , optimized_clear_value_(optimized_clear_value)
    , mem_range_(mem_range)
    , external_resource(false)
    , global_state_(initial_state, ff::dx12::resource_state::type_t::global, static_cast<size_t>(desc.DepthOrArraySize), static_cast<size_t>(desc.MipLevels))
    , tracker_(nullptr)
{
    assert(desc.Dimension != D3D12_RESOURCE_DIMENSION_UNKNOWN && desc.MipLevels * desc.DepthOrArraySize > 0);

    bool target = (desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0;
    if (target && this->optimized_clear_value_.Format == DXGI_FORMAT_UNKNOWN)
    {
        this->optimized_clear_value_ = { desc.Format };
    }

    if (allocate_mem_range && !mem_range)
    {
        bool buffer = (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
        D3D12_RESOURCE_ALLOCATION_INFO alloc_info = ff::dx12::device()->GetResourceAllocationInfo(0, 1, &this->desc_);
        ff::dx12::mem_allocator& allocator = buffer ? ff::dx12::static_buffer_allocator() : (target ? ff::dx12::target_allocator() : ff::dx12::texture_allocator());
        this->mem_range_ = std::make_shared<ff::dx12::mem_range>(allocator.alloc_bytes(alloc_info.SizeInBytes, alloc_info.Alignment));
    }

    this->reset();
    assert(*this && (!mem_range || mem_range == this->mem_range_));

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::resource);
}

ff::dx12::resource::resource(std::string_view name, resource& other, ff::dx12::commands* commands)
    : resource(name, other.desc_, other.optimized_clear_value_)
{
    std::unique_ptr<ff::dx12::commands> new_commands = ::get_copy_commands(commands);
    commands->copy_resource(*this, other);
}

ff::dx12::resource::resource(resource&& other) noexcept
    : tracker_(nullptr)
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
        std::swap(this->residency_data_, other.residency_data_);
        std::swap(this->optimized_clear_value_, other.optimized_clear_value_);
        std::swap(this->name_, other.name_);
        std::swap(this->desc_, other.desc_);
        std::swap(this->global_state_, other.global_state_);
        std::swap(this->global_reads_, other.global_reads_);
        std::swap(this->global_write_, other.global_write_);
        std::swap(this->tracker_, other.tracker_);

        if (this->tracker_)
        {
            this->tracker_->resource_moved(other, *this);
        }
    }

    return *this;
}

ff::dx12::resource::operator bool() const
{
    return this->resource_ != nullptr;
}

const std::string& ff::dx12::resource::name() const
{
    return this->name_;
}

const D3D12_GPU_VIRTUAL_ADDRESS ff::dx12::resource::gpu_address() const
{
    return *this ? this->resource_->GetGPUVirtualAddress() : 0;
}

const std::shared_ptr<ff::dx12::mem_range>& ff::dx12::resource::mem_range() const
{
    return this->mem_range_;
}

const D3D12_RESOURCE_DESC& ff::dx12::resource::desc() const
{
    return this->desc_;
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

D3D12_CLEAR_VALUE ff::dx12::resource::optimized_clear_value() const
{
    return this->optimized_clear_value_;
}

ff::dx12::resource_state& ff::dx12::resource::global_state()
{
    return this->global_state_;
}

void ff::dx12::resource::tracker(ff::dx12::resource_tracker* value)
{
    this->tracker_ = value;
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
        if (this->global_write_)
        {
            wait_before_execute.add(this->global_write_);
            this->global_write_ = {};
        }

        this->global_reads_.add(next_fence_value);
    }

    tracker.state(*this, state, array_start, array_size, mip_start, mip_size);
}

ff::dx12::fence_value ff::dx12::resource::update_buffer(ff::dx12::commands* commands, const void* data, uint64_t offset, uint64_t size)
{
    assert(this->desc_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

    if (size && data && offset + size <= this->desc_.Width)
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
    assert(this->desc_.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

    if (size && offset + size <= this->desc_.Width)
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
    assert(this->desc_.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
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
        if (!mem_range.cpu_data())
        {
            assert(false);
            continue;
        }

        dest_image.pixels = static_cast<uint8_t*>(mem_range.cpu_data());

        for (size_t y = 0; y < scan_lines; y++)
        {
            const uint8_t* src_row = src_image.pixels + (y * src_image.rowPitch);
            uint8_t* dest_row = dest_image.pixels + (y * dest_image.rowPitch);

            std::memcpy(dest_row, src_row, std::min(src_image.rowPitch, dest_image.rowPitch));
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
    assert(this->desc_.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

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

ff::dx12::residency_data* ff::dx12::resource::residency_data()
{
    if (this->residency_data_)
    {
        return this->residency_data_.get();
    }

    if (this->mem_range_)
    {
        return this->mem_range_->residency_data();
    }

    assert(this->external_resource);
    return nullptr;
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

    assert(!this->tracker_);
    this->tracker_ = nullptr;

    this->global_reads_.clear();
    this->global_write_ = {};
    this->residency_data_.reset();
    this->resource_.Reset();
}

void ff::dx12::resource::before_reset()
{
    this->destroy(true);
}

bool ff::dx12::resource::reset()
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;

    if (this->external_resource)
    {
        // this resource shouldn't be recreated (like a swap chain buffer)
        return true;
    }
    else if (!this->mem_range_)
    {
        Microsoft::WRL::ComPtr<ID3D12Pageable> pageable;
        D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
        bool starts_resident = true;

        if (ff::dx12::supports_create_heap_not_resident())
        {
            heap_flags = D3D12_HEAP_FLAG_CREATE_NOT_ZEROED | D3D12_HEAP_FLAG_CREATE_NOT_RESIDENT;
            starts_resident = false;
        }

        if (FAILED(ff::dx12::device()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            heap_flags,
            &this->desc_,
            this->global_state_.get(0).first,
            (this->optimized_clear_value_.Format != DXGI_FORMAT_UNKNOWN) ? &this->optimized_clear_value_ : nullptr,
            IID_PPV_ARGS(&resource))) ||
            FAILED(resource.As(&this->resource_)) ||
            FAILED(resource.As(&pageable)))
        {
            return false;
        }

        D3D12_RESOURCE_ALLOCATION_INFO alloc_info = ff::dx12::device()->GetResourceAllocationInfo(0, 1, &this->desc_);
        this->residency_data_ = std::make_unique<ff::dx12::residency_data>(this->name_, this, std::move(pageable), alloc_info.SizeInBytes, starts_resident);
    }
    else
    {
        if (FAILED(ff::dx12::device()->CreatePlacedResource(
            ff::dx12::get_heap(*this->mem_range_->heap()),
            this->mem_range_->start(),
            &this->desc_,
            this->global_state_.get(0).first,
            (this->optimized_clear_value_.Format != DXGI_FORMAT_UNKNOWN) ? &this->optimized_clear_value_ : nullptr,
            IID_PPV_ARGS(&resource))) ||
            FAILED(resource.As(&this->resource_)))
        {
            return false;
        }
    }

    if (this->resource_)
    {
        this->resource_->SetName(ff::string::to_wstring(this->name_).c_str());
        return true;
    }

    return false;
}
