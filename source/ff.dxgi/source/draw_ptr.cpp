#include "pch.h"
#include "draw_base.h"
#include "draw_ptr.h"

ff::dxgi::draw_ptr::draw_ptr(ff::dxgi::draw_base* draw)
    : draw(draw)
{}

ff::dxgi::draw_ptr::draw_ptr(draw_ptr&& other) noexcept
    : draw(other.draw)
{
    other.draw = nullptr;
}

ff::dxgi::draw_ptr::~draw_ptr()
{
    this->reset();
}

void ff::dxgi::draw_ptr::reset()
{
    if (this->draw)
    {
        this->draw->end_draw();
        this->draw = nullptr;
    }
}

ff::dxgi::draw_ptr& ff::dxgi::draw_ptr::operator=(ff::dxgi::draw_base* draw)
{
    if (this->draw != draw)
    {
        this->reset();
        this->draw = draw;
    }

    return *this;
}

ff::dxgi::draw_ptr& ff::dxgi::draw_ptr::operator=(draw_ptr&& other) noexcept
{
    if (this != &other)
    {
        *this = other.draw;
        other.draw = nullptr;
    }

    return *this;
}

ff::dxgi::draw_ptr::operator bool() const
{
    return this->draw != nullptr;
}

bool ff::dxgi::draw_ptr::operator!() const
{
    return !this->draw;
}

ff::dxgi::draw_ptr::operator ff::dxgi::draw_base* () const
{
    return this->draw;
}

ff::dxgi::draw_base& ff::dxgi::draw_ptr::operator*() const
{
    assert(this->draw);
    return *this->draw;
}

ff::dxgi::draw_base* ff::dxgi::draw_ptr::operator->() const
{
    return this->draw;
}

