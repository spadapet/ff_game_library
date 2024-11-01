#include "pch.h"
#include "dx12/target_access.h"

ff::dx12::target_access& ff::dx12::target_access::get(ff::dxgi::target_base& obj)
{
    return static_cast<ff::dx12::target_access&>(obj.target_access());
}
