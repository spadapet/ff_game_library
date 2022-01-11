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

ff::dx12::residency_data::residency_data(std::string_view name, ff::dx12::residency_access* owner, Microsoft::WRL::ComPtr<ID3D12Pageable>&& pageable, uint64_t size, bool resident)
    : name(name)
    , owner(owner)
    , pageable(std::move(pageable))
    , size(size)
    , resident(resident)
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

bool ff::dx12::residency_data::make_resident(const std::unordered_set<ff::dx12::residency_data*>& residency_set, ff::dx12::fence_value commands_fence_value, ff::dx12::fence_values& wait_values)
{
    ff::stack_vector<ID3D12Pageable*, 256> make_resident;
    ff::stack_vector<ID3D12Pageable*, 256> make_evicted;
    ff::dx12::fence_value resident_fence_value = ff::dx12::residency_fence().signal_later();
    uint64_t make_resident_size = 0;
    uint64_t make_evicted_size = 0;
    bool make_resident_succeeded = true;

    const uint64_t available_space = ff::dx12::get_video_memory_info().Budget - ff::dx12::get_video_memory_info().CurrentUsage;
    const uint32_t new_usage_counter = ::usage_counter.fetch_add(1) + 1;

    // Find data that needs to be made resident
    for (ff::dx12::residency_data* data : residency_set)
    {
        data->usage_counter = new_usage_counter;

        if (!data->resident)
        {
            make_resident.push_back(data->pageable.Get());
            make_resident_size += data->size;

            data->resident = true;
            data->resident_value = resident_fence_value;

            ff::log::write(ff::log::type::dx12_residency, "Make data resident:", static_cast<void*>(data), ", name:", data->name);
        }
        else if (data->resident_value.complete())
        {
            data->resident_value = {};
        }
        else
        {
            // Still becoming resident from a different call to make_resident
            wait_values.add(data->resident_value);
        }
    }

    // Move used data to the front of MRU
    {
        std::scoped_lock lock(::pageable_mutex);

        for (ff::dx12::residency_data* data : residency_set)
        {
            ff::intrusive_list::remove(::pageable_front, ::pageable_back, data);
            ff::intrusive_list::add_front(::pageable_front, ::pageable_back, data);
        }
    }

    // Evict LRU until below budget. This could do two passes:
    // 1) Evict data that isn't being used by the GPU at the moment
    // 2) Wait for GPU work to finish, then any resident data should be able to be evicted
    {
        ff::dx12::fence_values wait_to_evict;
        uint64_t delta_resident_size = make_resident_size;
        static const bool debug_residency = false; // DEBUG; // evict everything that isn't needed right now
        {
            std::scoped_lock lock(::pageable_mutex);

            for (ff::dx12::residency_data* data = ::pageable_back;
                data && data->usage_counter != new_usage_counter && (delta_resident_size > available_space || debug_residency);
                data = data->intrusive_prev_)
            {
                if (data->resident)
                {
                    data->resident = false;
                    data->resident_value = {};

                    wait_to_evict.add(data->keep_resident);
                    make_evicted.push_back(data->pageable.Get());
                    make_evicted_size += data->size;
                    delta_resident_size -= std::min(data->size, delta_resident_size);

                    ff::log::write(ff::log::type::dx12_residency, "Evict data:", static_cast<void*>(data), ", name:", data->name);
                }
            }
        }

        if (delta_resident_size > available_space && !debug_residency)
        {
            ff::log::write_debug_fail(ff::log::type::dx12_residency, "Over budget by:", delta_resident_size - available_space, " bytes, Available:", available_space, " bytes");
        }

        wait_to_evict.wait(nullptr);
    }

    if (!make_evicted.empty())
    {
        ff::log::write(ff::log::type::dx12_residency, "Evicting:", make_evicted_size, " bytes, Allocation count:", make_evicted.size());
        ff::dx12::device()->Evict(static_cast<UINT>(make_evicted.size()), make_evicted.data());
    }

    if (!make_resident.empty())
    {
        ff::log::write(ff::log::type::dx12_residency, "Making resident:", make_resident_size, " bytes, Allocation count:", make_resident.size());

        if (SUCCEEDED(ff::dx12::device()->EnqueueMakeResident(
            D3D12_RESIDENCY_FLAG_NONE,
            static_cast<UINT>(make_resident.size()),
            make_resident.data(),
            ff::dx12::get_fence(*resident_fence_value.fence()),
            resident_fence_value.get())))
        {
            wait_values.add(resident_fence_value);
        }
        else
        {
            ff::log::write_debug_fail(ff::log::type::dx12_residency, "Failed to make enough data resident");
            make_resident_succeeded = false;
        }
    }

    for (ff::dx12::residency_data* data : residency_set)
    {
        if (make_resident_succeeded)
        {
            data->keep_resident.add(commands_fence_value);
        }
        else if (data->resident && data->resident_value == resident_fence_value)
        {
            // Couldn't make the data resident, go back to being evicted
            data->resident = false;
            data->resident_value = {};
        }
    }

    return make_resident_succeeded;
}
