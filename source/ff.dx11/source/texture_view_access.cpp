#include "pch.h"
#include "texture_view_access.h"

ff::dx11::texture_view_access& ff::dx11::texture_view_access::get(ff::dxgi::texture_view_base& obj)
{
    return static_cast<ff::dx11::texture_view_access&>(obj.view_access());
}

