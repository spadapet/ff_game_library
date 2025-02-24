#include "pch.h"
#include "windows/window_types.h"

ff::point_size ff::window_size::physical_pixel_size() const
{
    return this->logical_to_physical_size(this->logical_pixel_size);
}

int ff::window_size::rotated_degrees(bool ccw) const
{
    return ccw ? (360 - this->rotation * 90) % 360 : this->rotation * 90;
}
