#include "pch.h"
#include "dx12_command_queue.h"
#include "dx12_commands.h"
#include "graphics.h"

#if DXVER == 12

static constexpr uint64_t type_to_fence(D3D12_COMMAND_LIST_TYPE type)
{
    return (static_cast<uint64_t>(type) + 1) << 56;
}

static constexpr D3D12_COMMAND_LIST_TYPE fence_to_type(uint64_t value)
{
    return static_cast<D3D12_COMMAND_LIST_TYPE>((value >> 56) - 1);
}

ff::dx12_command_queue::dx12_command_queue(dx12_command_queues& owner, D3D12_COMMAND_LIST_TYPE type, uint64_t initial_fence_value)
    : owner(owner)
    , type(type)
    , fence_event(ff::create_event(false, false))
    , completed_fence_value(initial_fence_value)
    , next_fence_value(initial_fence_value + 1)
{
    const D3D12_COMMAND_QUEUE_DESC command_queue_desc{ type };
    ff::graphics::dx12_device()->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&this->command_queue));
    ff::graphics::dx12_device()->CreateFence(this->completed_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->fence));

    ff::internal::graphics::add_child(this);
}

ff::dx12_command_queue::~dx12_command_queue()
{
    this->wait_for_idle();

    ff::internal::graphics::remove_child(this);
}

ID3D12CommandQueueX* ff::dx12_command_queue::get() const
{
    return this->command_queue.Get();
}

uint64_t ff::dx12_command_queue::signal_fence()
{
    std::lock_guard<std::mutex> lock(this->next_fence_mutex);
    this->command_queue->Signal(this->fence.Get(), this->next_fence_value);
    return this->next_fence_value++;
}

bool ff::dx12_command_queue::fence_complete(uint64_t value)
{
    std::lock_guard<std::mutex> lock(this->completed_fence_mutex);
    return this->internal_fence_complete(value);
}

bool ff::dx12_command_queue::internal_fence_complete(uint64_t value)
{
    if (value > this->completed_fence_value)
    {
        this->completed_fence_value = std::max(this->completed_fence_value, this->fence->GetCompletedValue());
    }

    return value <= this->completed_fence_value;
}

void ff::dx12_command_queue::wait_for_fence(uint64_t value)
{
    if (value)
    {
        ff::dx12_command_queue& other = this->owner.from_fence(value);
        if (this == &other)
        {
            std::lock_guard<std::mutex> lock(this->completed_fence_mutex);

            if (!this->internal_fence_complete(value) && SUCCEEDED(this->fence->SetEventOnCompletion(value, this->fence_event)))
            {
                ::WaitForSingleObject(this->fence_event, INFINITE);
                this->completed_fence_value = value;
            }
        }
        else
        {
            other.get()->Wait(other.fence.Get(), value);
        }
    }
}

void ff::dx12_command_queue::wait_for_idle()
{
    this->wait_for_fence(this->signal_fence());
}

ff::dx12_commands ff::dx12_command_queue::new_commands(ID3D12PipelineStateX* initial_state)
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
    {
        std::lock_guard<std::mutex> lock(this->allocators_mutex);

        if (this->allocators.empty() || !this->fence_complete(this->allocators.front().first))
        {
            ff::graphics::dx12_device()->CreateCommandAllocator(this->type, IID_PPV_ARGS(&allocator));
        }
        else
        {
            allocator = std::move(this->allocators.front().second);
            this->allocators.pop_front();
        }
    }

    Microsoft::WRL::ComPtr < ID3D12GraphicsCommandListX> list;
    {
        std::lock_guard<std::mutex> lock(this->lists_mutex);

        if (this->lists.empty())
        {
            ff::graphics::dx12_device()->CreateCommandList1(0, this->type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&list));
        }
        else
        {
            list = std::move(this->lists.front());
            this->lists.pop_front();
        }
    }

    allocator->Reset();
    return ff::dx12_commands(*this, std::move(list), std::move(allocator), initial_state);
}

void ff::dx12_command_queue::return_commands(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX>&& list, Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX>&& allocator, uint64_t allocator_fence_value)
{
    if (allocator)
    {
        std::lock_guard<std::mutex> lock(this->allocators_mutex);
        if (!allocator_fence_value)
        {
            // didn't draw anything?
            this->allocators.push_front(std::make_pair(allocator_fence_value, std::move(allocator)));
        }
        else
        {
            this->allocators.push_back(std::make_pair(allocator_fence_value, std::move(allocator)));
        }
    }

    if (list)
    {
        std::lock_guard<std::mutex> lock(this->lists_mutex);
        this->lists.push_front(std::move(list));
    }
}

bool ff::dx12_command_queue::reset()
{
    // allocator
    {
        std::lock_guard<std::mutex> lock(this->allocators_mutex);
        this->allocators.clear();
    }

    // list
    {
        std::lock_guard<std::mutex> lock(this->lists_mutex);
        this->lists.clear();
    }

    const D3D12_COMMAND_QUEUE_DESC command_queue_desc{ this->type };
    this->command_queue.Reset();
    ff::graphics::dx12_device()->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&this->command_queue));

    this->fence.Reset();
    ff::graphics::dx12_device()->CreateFence(this->completed_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->fence));

    return true;
}

int ff::dx12_command_queue::reset_priority() const
{
    return ff::internal::graphics_reset_priorities::dx12_command_queue;
}

ff::dx12_command_queues::dx12_command_queues()
    : direct_queue(*this, D3D12_COMMAND_LIST_TYPE_DIRECT, ::type_to_fence(D3D12_COMMAND_LIST_TYPE_DIRECT))
    , compute_queue(*this, D3D12_COMMAND_LIST_TYPE_COMPUTE, ::type_to_fence(D3D12_COMMAND_LIST_TYPE_COMPUTE))
    , copy_queue(*this, D3D12_COMMAND_LIST_TYPE_COPY, ::type_to_fence(D3D12_COMMAND_LIST_TYPE_COPY))
{}

ff::dx12_command_queue& ff::dx12_command_queues::direct()
{
    return this->direct_queue;
}

ff::dx12_command_queue& ff::dx12_command_queues::compute()
{
    return this->compute_queue;
}

ff::dx12_command_queue& ff::dx12_command_queues::copy()
{
    return this->copy_queue;
}

ff::dx12_command_queue& ff::dx12_command_queues::from_type(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type)
    {
    default:
        return this->direct_queue;

    case D3D12_COMMAND_LIST_TYPE_COPY:
        return this->copy_queue;

    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        return this->compute_queue;
    }
}

ff::dx12_command_queue& ff::dx12_command_queues::from_fence(uint64_t value)
{
    return this->from_type(::fence_to_type(value));
}

void ff::dx12_command_queues::wait_for_fence(uint64_t value)
{
    this->from_fence(value).wait_for_fence(value);
}

void ff::dx12_command_queues::wait_for_idle()
{
    this->copy_queue.wait_for_idle();
    this->compute_queue.wait_for_idle();
    this->direct_queue.wait_for_idle();
}

#endif
