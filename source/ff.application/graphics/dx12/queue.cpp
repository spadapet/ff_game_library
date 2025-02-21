#include "pch.h"
#include "graphics/dx12/access.h"
#include "graphics/dx12/device_reset_priority.h"
#include "graphics/dx12/dx12_globals.h"
#include "graphics/dx12/fence.h"
#include "graphics/dx12/gpu_event.h"
#include "graphics/dx12/residency.h"
#include "graphics/dx12/resource_tracker.h"
#include "graphics/dx12/queue.h"

static ff::perf_counter perf_execute("Execute", ff::perf_color::green);

ff::dx12::queue::queue(std::string_view name, D3D12_COMMAND_LIST_TYPE type)
    : type(type)
    , name_(name)
    , idle_fence(this->name_ + " idle fence", this)
{
    this->reset();
    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::queue);
}

ff::dx12::queue::~queue()
{
    this->wait_for_tasks();

    ff::dx12::remove_device_child(this);
}

ff::dx12::queue::operator bool() const
{
    return this->command_queue != nullptr;
}

const std::string& ff::dx12::queue::name() const
{
    return this->name_;
}

void ff::dx12::queue::wait_for_idle()
{
    this->idle_fence.signal(this).wait(nullptr);
}

void ff::dx12::queue::begin_event(ff::dx12::gpu_event type)
{
    if constexpr (ff::constants::profile_build)
    {
        ::PIXBeginEvent(this->command_queue.Get(), ff::dx12::gpu_event_color(type), ff::dx12::gpu_event_name(type));
    }
}

void ff::dx12::queue::end_event()
{
    if constexpr (ff::constants::profile_build)
    {
        ::PIXEndEvent(this->command_queue.Get());
    }
}

std::unique_ptr<ff::dx12::commands> ff::dx12::queue::new_commands()
{
    // Access cache
    std::unique_ptr<ff::dx12::commands::data_cache_t> cache;
    {
        std::scoped_lock lock(this->mutex);

        if (!this->caches.empty() && this->caches.front()->lists_reset_event.is_set())
        {
            cache = std::move(this->caches.front());
            cache->lists_reset_event.reset();
            this->caches.pop_front();
        }
    }

    if (!cache)
    {
        cache = std::make_unique<ff::dx12::commands::data_cache_t>(this);
        this->new_allocators(cache->allocator, cache->allocator_before);

        static std::atomic_int list_counter;
        static std::atomic_int list_before_counter;

        if (SUCCEEDED(ff::dx12::device()->CreateCommandList(0, this->type, cache->allocator.Get(), nullptr, IID_PPV_ARGS(&cache->list))) &&
            SUCCEEDED(ff::dx12::device()->CreateCommandList(0, this->type, cache->allocator_before.Get(), nullptr, IID_PPV_ARGS(&cache->list_before))))
        {
            cache->list->SetName(ff::string::concatw(this->name_, " commands ", list_counter.fetch_add(1)).c_str());
            cache->list_before->SetName(ff::string::concatw(this->name_, " commands before ", list_before_counter.fetch_add(1)).c_str());
        }
        else
        {
            assert(false);
        }
    }

    return std::make_unique<ff::dx12::commands>(*this, std::move(cache));
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
    ff::perf_timer timer(::perf_execute);
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

    std::unordered_set<ff::dx12::residency_data*> residency_set;
    ff::dx12::fence_values wait_before_execute;
    ff::stack_vector<ID3D12CommandList*, 64> dx12_lists;
    ff::stack_vector<ff::dx12::commands::data_cache_t*, 32> caches_to_reset;
    ff::dx12::fence_values fence_values;
    ff::dx12::fence_value next_fence_value;

    dx12_lists.reserve(valid_commands.size() * 2);
    caches_to_reset.reserve(valid_commands.size());
    fence_values.reserve(valid_commands.size());

    for (size_t i = 0; i < valid_commands.size(); i++)
    {
        ff::dx12::commands* prev_commands = i ? valid_commands[i - 1] : nullptr;
        ff::dx12::commands* next_commands = (i + 1 < valid_commands.size()) ? valid_commands[i + 1] : nullptr;
        valid_commands[i]->close_command_lists(prev_commands, next_commands, wait_before_execute);
    }

    for (ff::dx12::commands* cur : valid_commands)
    {
        next_fence_value = cur->next_fence_value();
        fence_values.add(next_fence_value);

        std::unique_ptr<ff::dx12::commands::data_cache_t> cache = cur->take_data();
        auto allocator_data = std::make_pair(next_fence_value, std::move(cache->allocator));
        auto allocator_before_data = std::make_pair(next_fence_value, std::move(cache->allocator_before));
        dx12_lists.push_back(cache->list_before.Get());
        dx12_lists.push_back(cache->list.Get());
        residency_set.merge(cache->residency_set);
        cache->residency_set.clear();

        std::scoped_lock lock(this->mutex);
        this->allocators.push_back(std::move(allocator_data));
        this->allocators_before.push_back(std::move(allocator_before_data));
        this->caches.push_back(std::move(cache));
        caches_to_reset.push_back(this->caches.back().get());
    }

    bool all_resident = ff::dx12::residency_data::make_resident(residency_set, next_fence_value, wait_before_execute);
    wait_before_execute.wait(this);

    if (all_resident)
    {
        this->command_queue->ExecuteCommandLists(static_cast<UINT>(dx12_lists.size()), dx12_lists.data());
    }

    fence_values.signal(this);

    ff::thread_pool::add_task([this, caches_to_reset = std::move(caches_to_reset)]()
    {
        for (auto* cache_to_reset : caches_to_reset)
        {
            this->new_allocators(cache_to_reset->allocator, cache_to_reset->allocator_before);
            HRESULT hr1 = cache_to_reset->list->Reset(cache_to_reset->allocator.Get(), nullptr);
            HRESULT hr2 = cache_to_reset->list_before->Reset(cache_to_reset->allocator_before.Get(), nullptr);
            assert(SUCCEEDED(hr1) && SUCCEEDED(hr2));
            cache_to_reset->lists_reset_event.set();
        }
    });
}

void ff::dx12::queue::new_allocators(Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& allocator, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& allocator_before)
{
    assert(!allocator && !allocator_before);
    {
        std::scoped_lock lock(this->mutex);

        if (!this->allocators.empty() && this->allocators.front().first.complete())
        {
            allocator = std::move(this->allocators.front().second);
            this->allocators.pop_front();
        }

        if (!this->allocators_before.empty() && this->allocators_before.front().first.complete())
        {
            allocator_before = std::move(this->allocators_before.front().second);
            this->allocators_before.pop_front();
        }
    }

    static std::atomic_int allocator_counter;
    static std::atomic_int allocator_before_counter;

    if (allocator)
    {
        verify_hr(allocator->Reset());
    }
    else if (SUCCEEDED(ff::dx12::device()->CreateCommandAllocator(this->type, IID_PPV_ARGS(&allocator))))
    {
        allocator->SetName(ff::string::concatw(this->name_, " allocator ", allocator_counter.fetch_add(1)).c_str());
    }

    if (allocator_before)
    {
        verify_hr(allocator_before->Reset());
    }
    else if (SUCCEEDED(ff::dx12::device()->CreateCommandAllocator(this->type, IID_PPV_ARGS(&allocator_before))))
    {
        allocator_before->SetName(ff::string::concatw(this->name_, " allocator before ", allocator_before_counter.fetch_add(1)).c_str());
    }
}

void ff::dx12::queue::wait_for_tasks()
{
    std::vector<HANDLE> handles;
    {
        std::scoped_lock lock(this->mutex);

        handles.reserve(this->caches.size());

        for (const auto& i : this->caches)
        {
            if (!i->lists_reset_event.is_set())
            {
                handles.push_back(i->lists_reset_event);
            }
        }
    }

    ff::wait_for_all_handles(handles.data(), handles.size(), INFINITE, false);
}

void ff::dx12::queue::before_reset()
{
    this->wait_for_tasks();

    this->allocators.clear();
    this->allocators_before.clear();
    this->caches.clear();

    this->command_queue.Reset();
}

bool ff::dx12::queue::reset()
{
    const D3D12_COMMAND_QUEUE_DESC command_queue_desc{ this->type };
    assert_hr_ret_val(ff::dx12::device()->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&this->command_queue)), false);
    this->command_queue->SetName(ff::string::to_wstring(this->name_).c_str());
    return true;
}
