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

ff::point_int ff::window_size::rotated_pixel_size() const
{
    return (this->current_rotation & 1) != (this->native_rotation & 1)
        ? this->pixel_size.swap()
        : this->pixel_size;
}
