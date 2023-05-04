#include "pch.h"
#include "draw_device.h"
#include "globals.h"

std::unique_ptr<ff::dxgi::draw_device_base> ff::dx12::create_draw_device()
{
    return ff::dx12::supports_mesh_shaders()
        ? ff::internal::dx12::create_draw_device_ms()
        : ff::internal::dx12::create_draw_device_gs();
}
