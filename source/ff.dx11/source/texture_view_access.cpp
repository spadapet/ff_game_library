#include "pch.h"
#include "texture_view_access.h"

const ff::dx11::texture_view_access& ff::dx11::texture_view_access::get(const ff::dxgi::texture_view_base& obj)
{
    return static_cast<const ff::dx11::texture_view_access&>(obj.view_access());
}

