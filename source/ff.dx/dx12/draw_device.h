#pragma once

#include "../dxgi/draw_device_base.h"

namespace ff::internal::dx12
{
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device_gs();
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device_ms();
}

namespace ff::dx12
{
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device();
}
