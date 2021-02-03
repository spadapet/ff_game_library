#include "pch.h"
#include "window.h"

ff::point_int ff::window_size::rotated_pixel_size() const
{
    return (this->current_rotation & 1) != (this->native_rotation & 1)
        ? this->pixel_size.swap()
        : this->pixel_size;
}
