#include "pch.h"
#include "commands.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "mem_allocator.h"
#include "mem_range.h"
#include "queue.h"
#include "resource.h"

ff::dx12::resource::resource(
    const D3D12_RESOURCE_DESC& desc,
    D3D12_RESOURCE_STATES initial_state,
    D3D12_CLEAR_VALUE optimized_clear_value,
    std::shared_ptr<ff::dx12::mem_range> mem_range)
    : mem_range_(mem_range)
    , optimized_clear_value(optimized_clear_value)
    , state_(initial_state)
    , desc_(desc)
    , alloc_info_(ff::dx12::device()->GetResourceAllocationInfo(0, 1, &desc))
{
    assert(desc.Dimension != D3D12_RESOURCE_DIMENSION_UNKNOWN && this->alloc_info_.SizeInBytes > 0);

    if (!mem_range || ff::math::align_up(mem_range->start(), this->alloc_info_.Alignment) != mem_range->start() || mem_range->size() < this->alloc_info_.SizeInBytes)
    {
        ff::dx12::mem_allocator& allocator = (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) ? ff::dx12::static_buffer_allocator() : ff::dx12::texture_allocator();
        this->mem_range_ = std::make_shared<ff::dx12::mem_range>(allocator.alloc_bytes(this->alloc_info_.SizeInBytes, this->alloc_info_.Alignment));
    }

    this->reset();
    assert(*this && (!mem_range || mem_range == this->mem_range_));

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::resource);
}

ff::dx12::resource::resource(resource&& other) noexcept
    : state_(D3D12_RESOURCE_STATE_COMMON)
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
        std::swap(this->state_, other.state_);
        std::swap(this->desc_, other.desc_);
        std::swap(this->alloc_info_, other.alloc_info_);
        std::swap(this->read_fence_values, other.read_fence_values);
        std::swap(this->write_fence_value, other.write_fence_value);
    }

    return *this;
}

ff::dx12::resource::operator bool() const
{
    return this->resource_ && this->mem_range_;
}

void ff::dx12::resource::active(bool value, ff::dx12::commands* commands)
{
    if (!value != !this->active())
    {
        this->mem_range_->active_resource(value ? this : nullptr, commands);
    }
}

bool ff::dx12::resource::active() const
{
    return this->mem_range_ && this->mem_range_->active_resource() == this;
}

void ff::dx12::resource::activated()
{}

void ff::dx12::resource::deactivated()
{}

const std::shared_ptr<ff::dx12::mem_range>& ff::dx12::resource::mem_range() const
{
    return this->mem_range_;
}

D3D12_RESOURCE_STATES ff::dx12::resource::state(D3D12_RESOURCE_STATES state, ff::dx12::commands* commands)
{
    D3D12_RESOURCE_STATES state_before = this->state_;
    if (state != this->state_)
    {
        this->state_ = state;

        if (commands)
        {
            commands->resource_barrier(this, state_before, this->state_);
        }
    }

    return state_before;
}

D3D12_RESOURCE_STATES ff::dx12::resource::state() const
{
    return this->state_;
}

const D3D12_RESOURCE_DESC& ff::dx12::resource::desc() const
{
    return this->desc_;
}

const D3D12_RESOURCE_ALLOCATION_INFO& ff::dx12::resource::alloc_info() const
{
    return this->alloc_info_;
}

ff::dx12::fence_value ff::dx12::resource::update_buffer(ff::dx12::commands* commands, const void* data, uint64_t offset, uint64_t size)
{
    if (!size || !data || offset + size > this->alloc_info_.SizeInBytes)
    {
        assert(!size);
        return {};
    }

    std::unique_ptr<ff::dx12::commands> new_commands;
    if (!commands)
    {
        new_commands = std::make_unique<ff::dx12::commands>(ff::dx12::copy_queue().new_commands());
        commands = new_commands.get();
    }

    ff::dx12::mem_range mem_range = ff::dx12::upload_allocator().alloc_buffer(size, commands->next_fence_value());
    if (!mem_range || !mem_range.cpu_data())
    {
        assert(false);
        return {};
    }

    ::memcpy(mem_range.cpu_data(), data, static_cast<size_t>(size));

    commands->wait_before_execute().add(this->read_fence_values);
    commands->wait_before_execute().add(this->write_fence_value);

    D3D12_RESOURCE_STATES state_before = this->state(D3D12_RESOURCE_STATE_COPY_DEST, commands);
    commands->copy_buffer(this, offset, mem_range);
    this->state(state_before, commands);

    this->write_fence_value = new_commands
        ? ff::dx12::copy_queue().execute(*new_commands)
        : commands->next_fence_value();

    return this->write_fence_value;
}

ff::dx12::fence_value ff::dx12::resource::update_texture(ff::dx12::commands* commands, const DirectX::ScratchImage& data)
{
    return ff::dx12::fence_value();
}

ff::dx12::fence_value ff::dx12::resource::update_texture(ff::dx12::commands* commands, const DirectX::Image& data, size_t sub_index, const D3D12_BOX* dest_box)
{
    return ff::dx12::fence_value();
}

std::pair<ff::dx12::fence_value, ff::dx12::mem_range> ff::dx12::resource::readback_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size)
{
    if (!size || offset + size > this->alloc_info_.SizeInBytes)
    {
        assert(!size);
        return {};
    }

    std::unique_ptr<ff::dx12::commands> new_commands;
    if (!commands)
    {
        new_commands = std::make_unique<ff::dx12::commands>(ff::dx12::copy_queue().new_commands());
        commands = new_commands.get();
    }

    ff::dx12::mem_range mem_range = ff::dx12::readback_allocator().alloc_buffer(size, commands->next_fence_value());
    if (!mem_range || !mem_range.cpu_data())
    {
        assert(false);
        return {};
    }

    commands->wait_before_execute().add(this->write_fence_value);

    D3D12_RESOURCE_STATES state_before = this->state(D3D12_RESOURCE_STATE_COPY_SOURCE, commands);
    commands->copy_buffer(mem_range, this, offset, size);
    this->state(state_before, commands);

    ff::dx12::fence_value fence_value = new_commands
        ? ff::dx12::copy_queue().execute(*new_commands)
        : commands->next_fence_value();

    this->read_fence_values.add(fence_value);
    return std::make_pair(std::move(fence_value), std::move(mem_range));
}

std::vector<uint8_t> ff::dx12::resource::capture_buffer(ff::dx12::commands* commands, uint64_t offset, uint64_t size)
{
    auto [fence_value, result_mem_range] = this->readback_buffer(commands, offset, size);
    if (!fence_value)
    {
        assert(!size);
        return {};
    }

    std::vector<uint8_t> result_bytes;
    result_bytes.resize(static_cast<size_t>(size));
    fence_value.wait(nullptr);
    std::memcpy(result_bytes.data(), result_mem_range.cpu_data(), size);

    return result_bytes;
}

ff::dx12::fence_value ff::dx12::resource::capture_texture(ff::dx12::commands* commands, const std::shared_ptr<DirectX::ScratchImage>& result)
{
    return ff::dx12::fence_value();
}

ff::dx12::fence_value ff::dx12::resource::capture_texture(ff::dx12::commands* commands, const std::shared_ptr<DirectX::ScratchImage>& result, size_t sub_index, const D3D12_BOX* box)
{
    return ff::dx12::fence_value();
}

void ff::dx12::resource::destroy(bool for_reset)
{
    if (this->active())
    {
        this->mem_range_->active_resource(nullptr, nullptr);
    }

    if (!for_reset)
    {
        ff::dx12::fence_values fence_values = std::move(this->read_fence_values);
        fence_values.add(this->write_fence_value);
        this->write_fence_value = {};

        ff::dx12::keep_alive_resource(std::move(*this), std::move(fence_values));

        this->mem_range_.reset();
    }
    else
    {
        this->read_fence_values.clear();
        this->write_fence_value = {};
    }

    this->resource_.Reset();
}

void ff::dx12::resource::before_reset()
{
    this->destroy(true);
}

bool ff::dx12::resource::reset()
{
    if (!this->mem_range_ ||
        FAILED(ff::dx12::device()->CreatePlacedResource(
            ff::dx12::get_heap(*this->mem_range_->heap()),
            this->mem_range_->start(),
            &this->desc_,
            this->state_,
            (this->optimized_clear_value.Format != DXGI_FORMAT_UNKNOWN) ? &this->optimized_clear_value : nullptr,
            IID_PPV_ARGS(&this->resource_))))
    {
        return false;
    }

    return true;
}
