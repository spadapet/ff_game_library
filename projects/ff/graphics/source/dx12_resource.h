#pragma once

#if DXVER == 12

namespace ff
{
    struct dx12_resource
    {
        Microsoft::WRL::ComPtr<ID3D12ResourceX> resource;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        uint64_t copy_fence_value;
    };
}

#endif
