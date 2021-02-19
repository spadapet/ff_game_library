#pragma once

namespace ff
{
    class draw_base;

    class draw_ptr
    {
    public:
        draw_ptr(ff::draw_base* draw);
        draw_ptr(draw_ptr&& other) noexcept;
        draw_ptr(const draw_ptr& other) = delete;
        ~draw_ptr();

        void reset();

        draw_ptr& operator=(ff::draw_base* draw);
        draw_ptr& operator=(draw_ptr&& other) noexcept;
        draw_ptr& operator=(const draw_ptr& other) = delete;
        operator bool() const;
        bool operator!() const;

        operator ff::draw_base* () const;
        ff::draw_base& operator*() const;
        ff::draw_base* operator->() const;

    private:
        ff::draw_base* draw;
    };
}
