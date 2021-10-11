#include "pch.h"
#include "target_access.h"

ff::dx11::target_access& ff::dx11::target_access::get(ff::dxgi::target_base& obj)
{
    return static_cast<ff::dx11::target_access&>(obj.target_access());
}

