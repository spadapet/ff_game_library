#include "pch.h"
#include "fence.h"
#include "fence_values.h"
#include "globals.h"

ff::dx12::fence_values::fence_values(std::initializer_list<ff::dx12::fence_value> list)
{
    this->values_.reserve(list.size());

    for (const ff::dx12::fence_value& value : list)
    {
        this->add(value);
    }
}

void ff::dx12::fence_values::add(ff::dx12::fence_value fence_value)
{
    if (fence_value)
    {
        for (ff::dx12::fence_value& value : this->values_)
        {
            if (value.fence() == fence_value.fence())
            {
                if (value.get() < fence_value.get())
                {
                    value = fence_value;
                }

                return;
            }
        }

        this->values_.push_back(fence_value);
    }
}

void ff::dx12::fence_values::add(const ff::dx12::fence_values& fence_values)
{
    this->values_.reserve(fence_values.values_.size() + this->values_.size());

    for (const ff::dx12::fence_value& value : fence_values.values_)
    {
        this->add(value);
    }
}

void ff::dx12::fence_values::reserve(size_t count)
{
    this->values_.reserve(count);
}

void ff::dx12::fence_values::clear()
{
    this->values_.clear();
}

bool ff::dx12::fence_values::empty() const
{
    return this->values_.empty();
}

void ff::dx12::fence_values::signal(ff::dx12::queue* queue)
{
    for (ff::dx12::fence_value& value : this->values_)
    {
        value.signal(queue);
    }
}

void ff::dx12::fence_values::wait(ff::dx12::queue* queue)
{
    if (queue)
    {
        for (ff::dx12::fence_value& value : this->values_)
        {
            value.wait(queue);
        }
    }
    else
    {
        ff::stack_vector<uint64_t, 16> actual_values;
        ff::stack_vector<ID3D12Fence*, 16> actual_fences;

        actual_values.reserve(this->values_.size());
        actual_fences.reserve(this->values_.size());

        for (ff::dx12::fence_value& value : this->values_)
        {
            if (!value.complete())
            {
                actual_values.push_back(value.get());
                actual_fences.push_back(ff::dx12::get_fence(*value.fence()));
            }
        }

        if (!actual_fences.empty())
        {
            ff::dx12::device()->SetEventOnMultipleFenceCompletion(
                actual_fences.data(),
                actual_values.data(),
                static_cast<UINT>(actual_fences.size()),
                D3D12_MULTIPLE_FENCE_WAIT_FLAG_ALL,
                nullptr);
        }
    }
}

bool ff::dx12::fence_values::complete()
{
    for (ff::dx12::fence_value& value : this->values_)
    {
        if (!value.complete())
        {
            return false;
        }
    }

    return true;
}

const ff::stack_vector<ff::dx12::fence_value, 4>& ff::dx12::fence_values::values() const
{
    return this->values_;
}
