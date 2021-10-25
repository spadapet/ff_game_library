#pragma once

namespace ff::dxgi
{
    class draw_base;

    class draw_ptr
    {
    public:
        draw_ptr(ff::dxgi::draw_base* draw);
        draw_ptr(draw_ptr&& other) noexcept;
        draw_ptr(const draw_ptr& other) = delete;
        ~draw_ptr();

        void reset();

        draw_ptr& operator=(ff::dxgi::draw_base* draw);
        draw_ptr& operator=(draw_ptr&& other) noexcept;
        draw_ptr& operator=(const draw_ptr& other) = delete;
        operator bool() const;
        bool operator!() const;

        operator ff::dxgi::draw_base* () const;
        ff::dxgi::draw_base& operator*() const;
        ff::dxgi::draw_base* operator->() const;

    private:
        ff::dxgi::draw_base* draw;
    };
}
