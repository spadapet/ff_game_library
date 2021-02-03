#include "pch.h"
#include "render_target_base.h"
#include "viewport.h"

ff::viewport::viewport(ff::point_int aspect, ff::rect_int padding)
    : aspect(aspect.cast<float>().abs())
    , padding(padding.cast<float>())
    , last_view{}
    , last_target_size{}
{}

ff::rect_int ff::viewport::view(ff::point_int target_size)
{
    if (target_size != this->last_target_size)
    {
        ff::rect_float safe_area = ff::rect_float(ff::point_float::zeros(), target_size.cast<float>());
        if (safe_area.width() > this->padding.left + this->padding.right && safe_area.height() > this->padding.top + this->padding.bottom)
        {
            // Adjust for padding
            safe_area = safe_area.deflate(this->padding.top_left(), this->padding.bottom_right());
        }

        this->last_view = (this->aspect.x * this->aspect.y > 0) ? ff::rect_float(ff::point_float::zeros(), this->aspect.abs()) : safe_area;
        this->last_view = this->last_view.scale_to_fit(safe_area);
        this->last_view = this->last_view.center(safe_area);
        this->last_target_size = target_size;
    }

    return this->last_view.cast<int>();
}
