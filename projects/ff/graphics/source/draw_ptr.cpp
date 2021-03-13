#include "pch.h"
#include "draw_base.h"
#include "draw_ptr.h"

ff::draw_ptr::draw_ptr(ff::draw_base* draw)
    : draw(draw)
{}

ff::draw_ptr::draw_ptr(draw_ptr&& other) noexcept
    : draw(other.draw)
{
    other.draw = nullptr;
}

ff::draw_ptr::~draw_ptr()
{
    this->reset();
}

void ff::draw_ptr::reset()
{
    if (this->draw)
    {
        this->draw->end_draw();
        this->draw = nullptr;
    }
}

ff::draw_ptr& ff::draw_ptr::operator=(ff::draw_base* draw)
{
    if (this->draw != draw)
    {
        this->reset();
        this->draw = draw;
    }

    return *this;
}

ff::draw_ptr& ff::draw_ptr::operator=(draw_ptr&& other) noexcept
{
    if (this != &other)
    {
        *this = other.draw;
        other.draw = nullptr;
    }

    return *this;
}

ff::draw_ptr::operator bool() const
{
    return this->draw != nullptr;
}

bool ff::draw_ptr::operator!() const
{
    return !this->draw;
}

ff::draw_ptr::operator ff::draw_base* () const
{
    return this->draw;
}

ff::draw_base& ff::draw_ptr::operator*() const
{
    assert(this->draw);
    return *this->draw;
}

ff::draw_base* ff::draw_ptr::operator->() const
{
    return this->draw;
}

