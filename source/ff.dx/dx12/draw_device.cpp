#include "pch.h"
#include "dx12/draw_device.h"
#include "dx12/globals.h"

std::unique_ptr<ff::dxgi::draw_device_base> ff::dx12::create_draw_device()
{
    return ff::dx12::supports_mesh_shaders()
        ? ff::internal::dx12::create_draw_device_ms()
        : ff::internal::dx12::create_draw_device_gs();
}
