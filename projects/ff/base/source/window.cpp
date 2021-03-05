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

int ff::window_size::rotated_degrees_from_native() const
{
    int rotation = (this->current_rotation < this->native_rotation ? this->current_rotation + 4 : this->current_rotation) - this->native_rotation;
    return rotation * 90;
}
