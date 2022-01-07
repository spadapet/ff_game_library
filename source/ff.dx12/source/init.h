#pragma once

namespace ff::dx12
{
    /// <summary>
    /// This is the entry point of the DX12 wrapper library, put this on the stack or just make sure
    /// it's created before calling any global functions in this library.
    /// </summary>
    class init
    {
    public:
        init(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0);
        ~init();

        operator bool() const;
        const ff::dxgi::client_functions& client_functions() const;

    private:
        ff::dxgi::client_functions client_functions_;
    };
}
