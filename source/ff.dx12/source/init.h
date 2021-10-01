#pragma once

namespace ff::dx12
{
    class init
    {
    public:
        init(D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0);
        ~init();

        operator bool() const;
    };
}
