#pragma once

namespace ff
{
    struct init_dx_params
    {
        DXGI_GPU_PREFERENCE gpu_preference{ DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE };
        D3D_FEATURE_LEVEL feature_level{ D3D_FEATURE_LEVEL_11_0 };
    };

    class init_dx
    {
    public:
        init_dx(const ff::init_dx_params& params);
        ~init_dx();

        operator bool() const;
    };

    class init_dx_async
    {
    public:
        init_dx_async(const ff::init_dx_params& params);
        ~init_dx_async();

        bool init_async() const;
        bool init_wait() const;
      
        operator bool() const;
    };
}
