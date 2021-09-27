#include "pch.h"
#include "access.h"
#include "fence.h"
#include "globals.h"
#include "queue.h"

ff::dx12::queue::queue(D3D12_COMMAND_LIST_TYPE type)
    : type(type)
{
    this->reset();
    ff::internal::dx12::add_device_child(this, ff::internal::dx12::device_reset_priority::queue);
}

ff::dx12::queue::~queue()
{
    ff::internal::dx12::remove_device_child(this);
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
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandListX> list;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocatorX> allocator;
    std::unique_ptr<ff::dx12::fence> fence;
    {
        std::scoped_lock lock(this->mutex);

        if (this->allocators.empty() || !this->allocators.front().first.complete())
        {
            ff::dx12::device()->CreateCommandAllocator(this->type, IID_PPV_ARGS(&allocator));
        }
        else
        {
            allocator = std::move(this->allocators.front().second);
            this->allocators.pop_front();
        }

        if (this->lists.empty())
        {
            ff::dx12::device()->CreateCommandList1(0, this->type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&list));
        }
        else
        {
            list = std::move(this->lists.front());
            this->lists.pop_front();
        }

        if (this->fences.empty())
        {
            fence = std::make_unique<ff::dx12::fence>(this);
        }
        else
        {
            fence = std::move(this->fences.front());
            this->fences.pop_front();
        }
    }

    allocator->Reset();
    return ff::dx12::commands(*this, list.Get(), allocator.Get(), std::move(fence), initial_state);
}

void ff::dx12::queue::execute(ff::dx12::commands& commands)
{
    ff::dx12::commands* p = &commands;
    this->execute(&p, 1);
}

void ff::dx12::queue::execute(ff::dx12::commands** commands, size_t count)
{
    ff::stack_vector<ID3D12CommandList*, 16> lists;
    ff::dx12::fence_values fence_values;
    ff::dx12::fence_values wait_before_execute;

    if (count)
    {
        lists.reserve(count);
        fence_values.reserve(count);

        for (size_t i = 0; i < count; i++)
        {
            ff::dx12::commands& cur = *commands[i];
            if (cur)
            {
                std::scoped_lock lock(this->mutex);
                this->allocators.push_back(std::make_pair(cur.next_fence_value(), ff::dx12::get_command_allocator(cur)));
                this->lists.push_front(ff::dx12::get_command_list(cur));
                this->fences.push_front(std::move(ff::dx12::move_fence(cur)));

                lists.push_back(this->lists.front().Get());
                fence_values.add(this->fences.front()->next_value());
            }

            wait_before_execute.add(cur.wait_before_execute());
            cur.close();
        }

        wait_before_execute.wait(this);

        if (!lists.empty())
        {
            this->command_queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
            fence_values.signal(this);
        }
    }
}

void ff::dx12::queue::before_reset()
{
    this->allocators.clear();
    this->lists.clear();
    this->fences.clear();

    this->command_queue.Reset();
}

bool ff::dx12::queue::reset()
{
    const D3D12_COMMAND_QUEUE_DESC command_queue_desc{ this->type };
    return SUCCEEDED(ff::dx12::device()->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&this->command_queue)));
}
