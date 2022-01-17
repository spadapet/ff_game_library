#include "pch.h"
#include "window.h"

bool ff::window_size::operator==(const ff::window_size& other) const
{
    return std::memcmp(this, &other, sizeof(other)) == 0;
}

bool ff::window_size::operator!=(const ff::window_size& other) const
{
    return std::memcmp(this, &other, sizeof(other)) != 0;
}

ff::point_size ff::window_size::rotated_pixel_size() const
{
    return (this->rotation & 1) != 0 ? this->pixel_size.swap() : this->pixel_size;
}

int ff::window_size::rotated_degrees(bool ccw) const
{
    return ccw ? (360 - this->rotation * 90) % 360 : this->rotation * 90;
}
