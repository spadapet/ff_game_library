#pragma once

#include "fence_values.h"

namespace ff::dx12
{
    class fence_values;

    enum class resident_t
    {
        evicted,
        resident,
    };

    class residency_data : public ff::intrusive_list::data<residency_data>
    {
    public:
        residency_data(Microsoft::WRL::ComPtr<ID3D12Pageable>&& pageable, uint64_t size, ff::dx12::resident_t residency);
        residency_data(residency_data&& other) noexcept = delete;
        residency_data(const residency_data& other) = delete;
        ~residency_data();

        residency_data& operator=(residency_data&& other) noexcept = delete;
        residency_data& operator=(const residency_data& other) = delete;

        void keep_resident(ff::dx12::fence_value value);

        static bool make_resident(const std::unordered_set<ff::dx12::residency_data*>& residency_set, ff::dx12::fence_values& wait_values);

    private:
        Microsoft::WRL::ComPtr<ID3D12Pageable> pageable_;
        uint64_t size_;
        ff::dx12::resident_t residency_;
        ff::dx12::fence_value resident_value;
        ff::dx12::fence_values keep_resident_;
        uint32_t usage_counter;
    };

    class residency_access
    {
    public:
        virtual ~residency_access() = default;

        virtual ff::dx12::residency_data* residency_data() = 0;
    };
}
