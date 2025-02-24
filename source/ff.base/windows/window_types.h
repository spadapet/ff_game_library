#pragma once

#include "../types/point.h"
#include "../types/rect.h"

namespace ff
{
    constexpr UINT WM_APP_FULL_SCREEN = WM_APP + 1; // wp = 0/1 (window/full)

    struct window_message
    {
        const HWND hwnd;
        const UINT msg;
        const WPARAM wp;
        const LPARAM lp;
        LRESULT result;
        bool handled;
    };

    struct window_placement
    {
        ff::rect_int normal_position;
        bool full_screen : 1;
        bool maximized : 1;
        bool minimized : 1;
    };

    struct window_size
    {
        bool operator==(const ff::window_size& other) const = default;

        ff::point_size physical_pixel_size() const; // as visible on screen
        int rotated_degrees(bool ccw = false) const;

        template<class T>
        ff::point_t<T> logical_scaled_size() const
        {
            return (this->logical_pixel_size.cast<double>() / this->dpi_scale).cast<T>();
        }

        template<class T>
        ff::rect_t<T> logical_pixel_rect() const
        {
            return ff::rect_t<T>({}, this->logical_pixel_size.cast<T>());
        }

        template<class T>
        ff::rect_t<T> logical_scaled_rect() const
        {
            return ff::rect_t<T>({}, this->logical_scaled_size<T>());
        }

        template<class T>
        ff::point_t<T> logical_to_physical_size(const ff::point_t<T>& size) const
        {
            return (this->rotation & 1) != 0 ? size.swap() : size;
        }

        template<class T>
        ff::point_t<T> physical_to_logical_size(const ff::point_t<T>& size) const
        {
            return (this->rotation & 1) != 0 ? size.swap() : size;
        }

        template<class T>
        ff::rect_t<T> logical_to_physical_rect(const ff::rect_t<T>& rect) const
        {
            const ff::point_t<T> size = this->logical_pixel_size.cast<T>();
            switch (this->rotation)
            {
                default: return rect;
                case DMDO_90: return { rect.top, size.x - rect.right, rect.bottom, size.x - rect.left };
                case DMDO_180: return { size.x - rect.right, size.y - rect.bottom, size.x - rect.left, size.y - rect.top };
                case DMDO_270: return { size.y - rect.bottom, rect.left, size.y - rect.top, rect.right };
            }
        }

        template<class T>
        ff::point_t<T> logical_to_physical_point(const ff::point_t<T>& point) const
        {
            return this->logical_to_physical_rect<T>({ point, point }).top_left();
        }

        template<class T>
        ff::rect_t<T> physical_to_logical_rect(const ff::rect_t<T>& rect) const
        {
            const ff::point_t<T> size = this->logical_pixel_size.cast<T>();
            switch (this->rotation)
            {
                default: return rect;
                case DMDO_90: return { size.y - rect.bottom, rect.left, size.y - rect.top, rect.right };
                case DMDO_180: return { size.x - rect.right, size.y - rect.bottom, size.x - rect.left, size.y - rect.top };
                case DMDO_270: return { rect.top, size.x - rect.right, rect.bottom, size.x - rect.left };
            }
        }

        template<class T>
        ff::point_t<T> physical_to_logical_point(const ff::point_t<T>& point) const
        {
            return this->physical_to_logical_rect<T>({ point, point }).top_left();
        }

        ff::point_size logical_pixel_size;
        double dpi_scale;
        int rotation; // DMDO_DEFAULT|90|180|270
    };
}
