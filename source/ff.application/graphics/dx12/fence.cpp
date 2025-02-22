#include "pch.h"
#include "graphics/dx12/access.h"
#include "graphics/dx12/device_reset_priority.h"
#include "graphics/dx12/dx12_globals.h"
#include "graphics/dx12/fence.h"
#include "graphics/dx12/queue.h"

ff::dx12::fence::fence(std::string_view name, ff::dx12::queue* queue, uint64_t initial_value)
    : queue_(queue)
    , name_(name)
    , wrapper_(std::make_shared<ff::dx12::fence_wrapper_t>(this))
    , completed_value(initial_value ? initial_value : 1)
    , next_value_(this->completed_value + 1)
{
    this->reset();

    ff::dx12::add_device_child(this, ff::dx12::device_reset_priority::fence);
}

ff::dx12::fence::~fence()
{
    ff::dx12::remove_device_child(this);

    this->wrapper_->fence = nullptr;
}

ff::dx12::fence::operator bool() const
{
    return this->fence_ != nullptr;
}

const std::string& ff::dx12::fence::name() const
{
    return this->name_;
}

ff::dx12::queue* ff::dx12::fence::queue() const
{
    return this->queue_;
}

ff::dx12::fence_value ff::dx12::fence::next_value()
{
    return ff::dx12::fence_value(this, this->next_value_);
}

ff::dx12::fence_value ff::dx12::fence::signal(ff::dx12::queue* queue)
{
    return this->signal(this->next_value_, queue);
}

ff::dx12::fence_value ff::dx12::fence::signal(uint64_t value, ff::dx12::queue* queue)
{
    assert(!queue || queue == this->queue_);

    if (*this && !this->complete(value))
    {
        this->next_value_ = value + 1;

        if (queue)
        {
            ff::log::write(ff::log::type::dx12_fence, "GPU signal fence:", this, ", value:", value, ", queue:", queue);
            ff::dx12::get_command_queue(*queue)->Signal(this->fence_.Get(), value);
        }
        else
        {
            ff::log::write(ff::log::type::dx12_fence, "CPU signal fence:", this, ", value:", value);
            this->fence_->Signal(value);
        }
    }

    return ff::dx12::fence_value(this, value);
}

ff::dx12::fence_value ff::dx12::fence::signal_later()
{
    return ff::dx12::fence_value(this, this->next_value_++);
}

void ff::dx12::fence::wait(uint64_t value, ff::dx12::queue* queue)
{
    if (*this && (!queue || queue != this->queue_) && !this->complete(value))
    {
        if (queue)
        {
            ff::log::write(ff::log::type::dx12_fence, "GPU wait fence:", this, ", value:", value, ", queue:", queue);
            ff::dx12::get_command_queue(*queue)->Wait(this->fence_.Get(), value);
        }
        else
        {
            ff::log::write(ff::log::type::dx12_fence, "CPU wait fence:", this, ", value:", value);

            ff::timer timer;

            if (SUCCEEDED(this->fence_->SetEventOnCompletion(value, nullptr)))
            {
                std::scoped_lock lock(this->completed_value_mutex);
                this->completed_value = std::max(this->completed_value, value);
            }

            ff::log::write(ff::log::type::dx12_fence, "CPU block on fence waited ", &std::fixed, std::setprecision(2), timer.tick() * 1000.0, "ms");
        }
    }
}

bool ff::dx12::fence::set_event(uint64_t value, HANDLE handle)
{
    if (handle)
    {
        ::ResetEvent(handle);

        if (!*this || this->complete(value) || FAILED(this->fence_->SetEventOnCompletion(value, handle)))
        {
            ::SetEvent(handle);
            return false;
        }

        return true;
    }
    else
    {
        this->wait(value, nullptr);
        return false;
    }
}

void ff::dx12::fence::wait(ff::dx12::fence_value* values, size_t count, ff::dx12::queue* queue)
{
    ff::stack_vector<uint64_t, 16> actual_values;
    ff::stack_vector<ff::dx12::fence*, 16> actual_wrappers;
    ff::stack_vector<ID3D12Fence*, 16> actual_fences;

    for (size_t i = 0; i < count; i++)
    {
        bool found = false;
        ff::dx12::fence_value& value = values[i];
        ff::dx12::fence* fence = value.fence();

        if ((!queue || queue != fence->queue_) && !value.complete())
        {
            for (size_t h = 0; h < actual_wrappers.size(); h++)
            {
                if (actual_wrappers[h] == fence)
                {
                    actual_values[h] = std::max(value.get(), actual_values[h]);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                actual_values.push_back(value.get());
                actual_wrappers.push_back(fence);
                actual_fences.push_back(ff::dx12::get_fence(*fence));
            }
        }
    }

    if (queue)
    {
        for (size_t i = 0; i < actual_wrappers.size(); i++)
        {
            actual_wrappers[i]->wait(actual_values[i], queue);
        }
    }
    else
    {
        ff::dx12::device()->SetEventOnMultipleFenceCompletion(
            actual_fences.data(),
            actual_values.data(),
            static_cast<UINT>(actual_fences.size()),
            D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL,
            nullptr);

        for (size_t i = 0; i < actual_wrappers.size(); i++)
        {
            verify(actual_wrappers[i]->complete(actual_values[i]));
        }
    }
}

bool ff::dx12::fence::complete(ff::dx12::fence_value* values, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!values[i].complete())
        {
            return false;
        }
    }

    return true;
}

bool ff::dx12::fence::complete(uint64_t value)
{
    std::scoped_lock lock(this->completed_value_mutex);
    if (value > this->completed_value)
    {
        this->completed_value = std::max(this->completed_value, this->fence_->GetCompletedValue());
    }

    return value <= this->completed_value;
}

std::shared_ptr<ff::dx12::fence_wrapper_t> ff::dx12::fence::wrapper() const
{
    return this->wrapper_;
}

void ff::dx12::fence::before_reset()
{
    this->completed_value = 1;
    this->next_value_ = this->completed_value + 1;
    this->fence_.Reset();
}

bool ff::dx12::fence::reset()
{
    assert_hr_ret_val(ff::dx12::device()->CreateFence(this->completed_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&this->fence_)), false);
    this->fence_->SetName(ff::string::to_wstring(this->name_).c_str());
    return true;
}
