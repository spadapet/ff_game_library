#pragma once

#include "../dxgi/draw_device_base.h"

namespace ff::dx12
{
    std::unique_ptr<ff::dxgi::draw_device_base> create_draw_device();
}
