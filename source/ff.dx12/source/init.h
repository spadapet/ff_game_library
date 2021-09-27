#pragma once

namespace ff
{
    class init_dx12
    {
    public:
        init_dx12(D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0);
        ~init_dx12();

        operator bool() const;
    };
}
