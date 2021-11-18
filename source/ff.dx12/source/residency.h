#pragma once

namespace ff::dx12
{
    class pageable_base
    {
    public:
        virtual ~pageable_base() = default;

        virtual size_t pageable_size() const = 0;
        virtual ID3D12Pageable* pageable() const = 0;
    };
}
