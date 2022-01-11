#pragma once

#include "fence_values.h"

namespace ff::dx12
{
    class fence_values;
    class residency_access;

    class residency_data : public ff::intrusive_list::data<residency_data>
    {
    public:
        residency_data(std::string_view name, ff::dx12::residency_access* owner, Microsoft::WRL::ComPtr<ID3D12Pageable>&& pageable, uint64_t size, bool resident);
        residency_data(residency_data&& other) noexcept = delete;
        residency_data(const residency_data& other) = delete;
        ~residency_data();

        residency_data& operator=(residency_data&& other) noexcept = delete;
        residency_data& operator=(const residency_data& other) = delete;

        static bool make_resident(const std::unordered_set<ff::dx12::residency_data*>& residency_set, ff::dx12::fence_value commands_fence_value, ff::dx12::fence_values& wait_values);

    private:
        std::string_view name;
        ff::dx12::residency_access* owner;
        Microsoft::WRL::ComPtr<ID3D12Pageable> pageable;
        uint64_t size;
        bool resident;
        ff::dx12::fence_value resident_value;
        ff::dx12::fence_values keep_resident;
        uint32_t usage_counter;
    };

    class residency_access
    {
    public:
        virtual ~residency_access() = default;

        virtual ff::dx12::residency_data* residency_data() = 0;
    };
}
