#pragma once

namespace ff
{
    class viewport
    {
    public:
        viewport(ff::point_int aspect, ff::rect_int padding = {});

        ff::rect_int view(ff::point_int target_size);

    private:
        ff::point_float aspect;
        ff::rect_float padding;
        ff::rect_float last_view;
        ff::point_int last_target_size;
    };
}
