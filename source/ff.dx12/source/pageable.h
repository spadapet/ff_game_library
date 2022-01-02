#pragma once

namespace ff::dx12
{
    class fence_values;

    class pageable_base : public ff::intrusive_list::data<pageable_base>
    {
    public:
        pageable_base();
        pageable_base(pageable_base&& other) noexcept;
        pageable_base(const pageable_base& other) = delete;
        virtual ~pageable_base();

        pageable_base& operator=(pageable_base&& other) noexcept;
        pageable_base& operator=(const pageable_base& other) = delete;

        static void make_resident(const std::unordered_set<ff::dx12::pageable_base*>& pageable_set, ff::dx12::fence_values& wait_values);

        virtual size_t pageable_size() const = 0;
        virtual ID3D12Pageable* dx12_pageable() const = 0;
    };

    class pageable_access
    {
    public:
        virtual ~pageable_access() = default;

        virtual ff::dx12::pageable_base* pageable() = 0;
    };
}
