#pragma once

namespace dxgi
{
    class client_functions;
    class host_functions;
}

namespace ff
{
    class init_dx
    {
    public:
        init_dx(const ff::dxgi::host_functions& host_functions, D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0);
        ~init_dx();

        operator bool() const;
        const ff::dxgi::client_functions& client_functions() const;
    };
}
