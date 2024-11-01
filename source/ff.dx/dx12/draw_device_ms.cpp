#include "pch.h"
#include "dx12/draw_device.h"

std::unique_ptr<ff::dxgi::draw_device_base> ff::internal::dx12::create_draw_device_ms()
{
    // Mesh shader not implemented
    return ff::internal::dx12::create_draw_device_gs();
}
