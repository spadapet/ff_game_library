#include "pch.h"
#include "access.h"
#include "fence.h"
#include "fence_values.h"
#include "globals.h"
#include "residency.h"

static std::mutex pageable_mutex;
static ff::dx12::residency_data* pageable_front{};
static ff::dx12::residency_data* pageable_back{};
static std::atomic_uint32_t usage_counter{ 0 };

ff::dx12::residency_data::residency_data(Microsoft::WRL::ComPtr<ID3D12Pageable>&& pageable, uint64_t size, ff::dx12::resident_t residency)
    : pageable_(std::move(pageable))
    , size_(size)
    , residency_(residency)
    , usage_counter(0)
{
    std::scoped_lock lock(::pageable_mutex);
    ff::intrusive_list::add_back(::pageable_front, ::pageable_back, this);
}

ff::dx12::residency_data::~residency_data()
{
    std::scoped_lock lock(::pageable_mutex);
    ff::intrusive_list::remove(::pageable_front, ::pageable_back, this);
}

void ff::dx12::residency_data::keep_resident(ff::dx12::fence_value value)
{
    this->keep_resident_.add(value);
}

bool ff::dx12::residency_data::make_resident(const std::unordered_set<ff::dx12::residency_data*>& residency_set, ff::dx12::fence_values& wait_values)
{
    ff::stack_vector<ID3D12Pageable*, 256> make_resident;
    ff::stack_vector<ID3D12Pageable*, 256> make_evicted;
    uint64_t make_resident_size = 0;
    uint64_t make_evicted_size = 0;

    const uint64_t available_space = ff::dx12::get_video_memory_info().Budget - ff::dx12::get_video_memory_info().CurrentUsage;
    const uint32_t new_usage_counter = ::usage_counter.fetch_add(1) + 1;

    // Move used data to the front of MRU and ensure the data made resident
    {
        std::scoped_lock lock(::pageable_mutex);

        for (ff::dx12::residency_data* data : residency_set)
        {
            if (data->residency_ == ff::dx12::resident_t::evicted)
            {
                make_resident.push_back(data->pageable_.Get());
                make_resident_size += data->size_;

                data->residency_ = ff::dx12::resident_t::resident;
                data->resident_value = ff::dx12::residency_fence().next_value();
            }
            else if (data->resident_value.complete())
            {
                data->resident_value = {};
            }
            else
            {
                wait_values.add(data->resident_value);
            }

            data->usage_counter = new_usage_counter;

            if (data != ::pageable_front)
            {
                ff::intrusive_list::remove(::pageable_front, ::pageable_back, data);
                ff::intrusive_list::add_front(::pageable_front, ::pageable_back, data);
            }
        }
    }

    // Evict LRU until below budget. This could do two passes:
    // 1) Evict data that isn't being used by the GPU at the moment
    // 2) Wait for GPU work to finish, then any resident data should be able to be evicted
    {
        uint64_t delta_resident_size = make_resident_size;
        ff::dx12::fence_values wait_to_evict;

        for (size_t pass = 0; pass < 2 && delta_resident_size > available_space; pass++)
        {
            if (pass == 1)
            {
                // Block the CPU until relevant GPU work is done
                wait_to_evict.wait(nullptr);
            }

            std::scoped_lock lock(::pageable_mutex);

            for (ff::dx12::residency_data* data = ::pageable_back;
                data && data->usage_counter != new_usage_counter && delta_resident_size > available_space;
                data = data->intrusive_prev_)
            {
                if (data->residency_ != ff::dx12::resident_t::evicted)
                {
                    if (data->keep_resident_.complete())
                    {
                        make_evicted.push_back(data->pageable_.Get());
                        data->residency_ = ff::dx12::resident_t::evicted;
                        data->resident_value = {};

                        make_evicted_size += data->size_;
                        delta_resident_size -= std::min(data->size_, delta_resident_size);
                    }
                    else
                    {
                        wait_to_evict.add(data->keep_resident_);
                    }
                }
            }
        }

        if (delta_resident_size > available_space)
        {
            ff::log::write_debug_fail(ff::log::type::dx12_residency, "Over budget by:", delta_resident_size - available_space, " bytes, Available:", available_space, " bytes");
        }
    }

    if (!make_evicted.empty())
    {
        ff::log::write(ff::log::type::dx12_residency, "Evicting:", make_evicted_size, " bytes, Allocation count:", make_evicted.size());
        ff::dx12::device()->Evict(static_cast<UINT>(make_evicted.size()), make_evicted.data());
    }

    if (!make_resident.empty())
    {
        ff::log::write(ff::log::type::dx12_residency, "Making resident:", make_resident_size, " bytes, Allocation count:", make_resident.size());
        ff::dx12::fence_value fence_value = ff::dx12::residency_fence().signal_later();

        if (FAILED(ff::dx12::device()->EnqueueMakeResident(
            D3D12_RESIDENCY_FLAG_NONE,
            static_cast<UINT>(make_resident.size()),
            make_resident.data(),
            ff::dx12::get_fence(*fence_value.fence()),
            fence_value.get())))
        {
            ff::log::write_debug_fail(ff::log::type::dx12_residency, "Failed to make enough data resident");

            for (ff::dx12::residency_data* data : residency_set)
            {
                // Couldn't make the data resident, go back to being evicted
                if (data->residency_ == ff::dx12::resident_t::resident && data->resident_value == fence_value)
                {
                    data->residency_ = ff::dx12::resident_t::evicted;
                    data->resident_value = {};
                }
            }

            // No need to signal the fence value, nothing will wait for it
            return false;
        }

        wait_values.add(fence_value);
    }

    return true;
}
