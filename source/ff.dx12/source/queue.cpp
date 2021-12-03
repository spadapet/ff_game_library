#include "pch.h"
#include "access.h"
#include "device_reset_priority.h"
#include "fence.h"
#include "globals.h"
#include "resource_tracker.h"
#include "queue.h"

ff::dx12::queue::queue(D3D12_COMMAND_LIST_TYPE type)
    : type(type)
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::queue);
}

ff::dx12::queue::~queue()
{
    ff::dx12::remove_device_child(this);
}

ff::dx12::queue::operator bool() const
{
    return this->command_queue != nullptr;
}

void ff::dx12::queue::wait_for_idle()
{
    ff::dx12::fence fence(this);
    fence.signal(this).wait(nullptr);
}

ff::dx12::commands ff::dx12::queue::new_commands(ID3D12PipelineStateX* initial_state)
{
    ff::dx12::commands::data_cache_t cache;
    std::scoped_lock lock(this->mutex);

    if (this->caches.empty())
    {
        ff::dx12::device()->CreateCommandList1(0, this->type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cache.list));
        ff::dx12::device()->CreateCommandList1(0, this->type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cache.list_before));
        cache.resource_tracker = std::make_unique<ff::dx12::resource_tracker>();
        cache.fence = std::make_unique<ff::dx12::fence>(this);
    }
    else
    {
        cache = std::move(this->caches.front());
        this->caches.pop_front();
    }

    if (this->allocators.empty() || !this->allocators.front().first.complete())
    {
        ff::dx12::device()->CreateCommandAllocator(this->type, IID_PPV_ARGS(&cache.allocator));
    }
    else
    {
        cache.allocator = std::move(this->allocators.front().second);
        this->allocators.pop_front();
    }

    cache.allocator->Reset();
    return ff::dx12::commands(*this, std::move(cache), initial_state);
}

ff::dx12::fence_value ff::dx12::queue::execute(ff::dx12::commands& commands)
{
    ff::dx12::fence_value fence_value = commands.next_fence_value();
    ff::dx12::commands* p = &commands;

    this->execute(&p, 1);

    return fence_value;
}

void ff::dx12::queue::execute(ff::dx12::commands** commands, size_t count)
{
    ff::stack_vector<ff::dx12::commands*, 32> valid_commands;
    valid_commands.reserve(count);

    for (size_t i = 0; i < count; i++)
    {
        if (commands[i] && *commands[i])
        {
            valid_commands.push_back(commands[i]);
        }
    }

    if (valid_commands.empty())
    {
        return;
    }

    ff::dx12::fence_values wait_before_execute;
    ff::stack_vector<ID3D12CommandList*, 64> dx12_lists;
    ff::dx12::fence_values fence_values;
    dx12_lists.reserve(valid_commands.size() * 2);
    fence_values.reserve(valid_commands.size());

    for (size_t i = 0; i < valid_commands.size(); i++)
    {
        ff::dx12::commands* prev_commands = i ? valid_commands[i - 1] : nullptr;
        ff::dx12::commands* next_commands = (i + 1 < valid_commands.size()) ? valid_commands[i + 1] : nullptr;
        valid_commands[i]->flush(prev_commands, next_commands, wait_before_execute);
    }

    for (ff::dx12::commands* cur : valid_commands)
    {
        ff::dx12::commands::data_cache_t cache = cur->close();
        dx12_lists.push_back(cache.list_before.Get());
        dx12_lists.push_back(cache.list.Get());

        ff::dx12::fence_value next_fence_value = cur->next_fence_value();
        fence_values.add(next_fence_value);

        std::scoped_lock lock(this->mutex);
        this->allocators.push_back(std::make_pair(next_fence_value, std::move(cache.allocator)));
        this->caches.push_front(std::move(cache));
    }

    wait_before_execute.wait(this);
    this->command_queue->ExecuteCommandLists(static_cast<UINT>(dx12_lists.size()), dx12_lists.data());
    fence_values.signal(this);
}

void ff::dx12::queue::before_reset()
{
    this->allocators.clear();
    this->caches.clear();

    this->command_queue.Reset();
}

bool ff::dx12::queue::reset()
{
    const D3D12_COMMAND_QUEUE_DESC command_queue_desc{ this->type };
    return SUCCEEDED(ff::dx12::device()->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&this->command_queue)));
}
