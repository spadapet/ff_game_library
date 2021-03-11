#include "pch.h"
#include "app.h"
#include "app_time.h"
#include "debug_state.h"

static std::vector<ff::debug_pages_base*> debug_pages;
static ff::signal<void()> custom_debug_signal;

void ff::add_debug_pages(ff::debug_pages_base* pages)
{
    if (pages && std::find(::debug_pages.cbegin(), ::debug_pages.cend(), pages) == ::debug_pages.cend())
    {
        ::debug_pages.push_back(pages);
    }
}

void ff::remove_debug_pages(ff::debug_pages_base* pages)
{
    auto i = std::find(::debug_pages.cbegin(), ::debug_pages.cend(), pages);
    if (i != ::debug_pages.cend())
    {
        ::debug_pages.erase(i);
    }
}

ff::signal_sink<void()>& ff::custom_debug_sink()
{
    return ::custom_debug_signal;
}

ff::debug_state::debug_state()
{
    ff::add_debug_pages(this);
}

ff::debug_state::~debug_state()
{
    ff::remove_debug_pages(this);
}

std::shared_ptr<ff::state> ff::debug_state::advance_time()
{
    return nullptr;
}

void ff::debug_state::advance_input()
{}

void ff::debug_state::render(ff::dx11_target_base& target, ff::dx11_depth& depth)
{
    if (!this->draw_device)
    {
        this->draw_device = ff::draw_device::create();
    }

    ff::rect_fixed rect(ff::point_fixed(0, 0), target.size().rotated_pixel_size().cast<ff::fixed_int>());
    ff::draw_ptr draw = this->draw_device->begin_draw(target, &depth, rect, rect);
    if (draw)
    {
        ff::fixed_int x = static_cast<int>(ff::app_time().advance_count % 256 + 32);
        draw->draw_outline_circle(ff::point_fixed(x, 32), 16, ff::color::red(), 4);
    }
}

void ff::debug_state::frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth)
{}

ff::state::status_t ff::debug_state::status()
{
    return ff::state::status_t::ignore;
}

size_t ff::debug_state::debug_page_count() const
{
    return 0;
}

std::string_view ff::debug_state::debug_page_name(size_t page) const
{
    return "";
}

void ff::debug_state::debug_page_update_stats(size_t page, bool update_fast_numbers)
{}

size_t ff::debug_state::debug_page_info_count(size_t page) const
{
    return 0;
}

void ff::debug_state::debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const
{}

size_t ff::debug_state::debug_page_toggle_count(size_t page) const
{
    return 0;
}

void ff::debug_state::debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const
{}

void ff::debug_state::debug_page_toggle(size_t page, size_t index)
{}
