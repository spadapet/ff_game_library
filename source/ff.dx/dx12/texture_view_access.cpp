#include "pch.h"
#include "dx12/texture_view_access.h"
#include "dxgi/texture_view_access_base.h"
#include "dxgi/texture_view_base.h"

ff::dx12::texture_view_access& ff::dx12::texture_view_access::get(ff::dxgi::texture_view_base& obj)
{
    return static_cast<ff::dx12::texture_view_access&>(obj.view_access());
}

