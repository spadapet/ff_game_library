#pragma once

namespace ff
{
    class viewport
    {
    public:
        viewport(ff::point_size aspect, ff::rect_size padding = {});

        ff::rect_int view(ff::point_size target_size);
        ff::rect_int last_view() const;

    private:
        ff::point_float aspect;
        ff::rect_float padding;
        ff::rect_float last_view_;
        ff::point_size last_target_size;
    };
}
