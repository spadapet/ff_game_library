#include "pch.h"
#include "dx12_descriptor_allocator.h"
#include "dx12_command_queue.h"
#include "dx12_commands.h"
#include "graphics.h"

#if DXVER == 12

ff::dx12_commands::dx12_commands(
    dx12_command_queue& owner,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX>&& list,
    Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX>&& allocator,
    ID3D12PipelineStateX* initial_state)
    : owner(&owner)
    , list(std::move(list))
    , allocator(std::move(allocator))
    , state_(initial_state)
    , allocator_fence_value(0)
    , open_(false)
{
    this->open();
}

ff::dx12_commands::dx12_commands(dx12_commands&& other) noexcept
    : owner(nullptr)
    , allocator_fence_value(0)
    , open_(false)
{
    *this = std::move(other);
}

ff::dx12_commands::~dx12_commands()
{
    this->destroy();
}

ff::dx12_commands& ff::dx12_commands::operator=(ff::dx12_commands&& other) noexcept
{
    if (this != &other)
    {
        this->destroy();

        std::swap(this->owner, other.owner);
        std::swap(this->list, other.list);
        std::swap(this->allocator, other.allocator);
        std::swap(this->state_, other.state_);
        std::swap(this->allocator_fence_value, other.allocator_fence_value);
        std::swap(this->open_, other.open_);
    }

    return *this;
}

ff::dx12_commands::operator bool() const
{
    return this->owner != nullptr;
}

ID3D12GraphicsCommandListX* ff::dx12_commands::get() const
{
    assert(this->open_);
    return this->list.Get();
}

ID3D12GraphicsCommandListX* ff::dx12_commands::operator->() const
{
    return this->get();
}

void ff::dx12_commands::state(ID3D12PipelineStateX* state)
{
    if (this->state_.Get() != state)
    {
        this->state_ = state;
        this->list->SetPipelineState(state);
    }
}

void ff::dx12_commands::transition(ID3D12ResourceX* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, size_t sub_resource)
{
    assert(this->open_);

    D3D12_RESOURCE_BARRIER barrier{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;

    this->list->ResourceBarrier(1, &barrier);
}

uint64_t ff::dx12_commands::execute(bool reopen)
{
    uint64_t fence_value = 0;

    if (this->close())
    {
        this->allocator_fence_value = fence_value = this->execute();

        if (reopen)
        {
            this->open();
        }
    }

    return fence_value;
}

bool ff::dx12_commands::reset()
{
    if (*this)
    {
        // Can't return the existing list/allocator
        this->list.Reset();
        this->allocator.Reset();

        *this = this->owner->new_commands();
    }

    return true;
}

int ff::dx12_commands::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_commands;
}

void ff::dx12_commands::destroy()
{
    if (*this)
    {
        this->close();
        this->owner->return_commands(std::move(this->list), std::move(this->allocator), this->allocator_fence_value);

        this->owner = nullptr;
        this->list.Reset();
        this->allocator.Reset();
        this->state_.Reset();
        this->allocator_fence_value = 0;
    }
}

uint64_t ff::dx12_commands::execute()
{
    uint64_t fence_value = 0;

    assert(!this->open_);
    if (!this->open_)
    {
        ID3D12CommandList* list = this->list.Get();
        this->owner->get()->ExecuteCommandLists(1, &list);
        fence_value = this->owner->signal_fence();
    }

    return fence_value;
}

bool ff::dx12_commands::close()
{
    if (this->open_)
    {
        this->list->Close();
        this->open_ = false;

        return true;
    }

    return false;
}

bool ff::dx12_commands::open()
{
    if (!this->open_)
    {
        this->list->Reset(this->allocator.Get(), this->state_.Get());

        ID3D12DescriptorHeap* heaps[2] =
        {
            ff::graphics::dx12_descriptors_gpu_buffer().get(),
            ff::graphics::dx12_descriptors_gpu_sampler().get(),
        };

        this->list->SetDescriptorHeaps(2, heaps);
        this->open_ = true;

        return true;
    }

    return false;
}

#endif
