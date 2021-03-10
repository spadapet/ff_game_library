#include "pch.h"
#include "debug_pages.h"

size_t ff::debug_pages_base::debug_page_count() const
{
    return 0;
}

std::string_view ff::debug_pages_base::debug_page_name(size_t page) const
{
    return "";
}

void ff::debug_pages_base::debug_page_update_stats(size_t page, bool update_fast_numbers)
{}

size_t ff::debug_pages_base::debug_page_info_count(size_t page) const
{
    return 0;
}

void ff::debug_pages_base::debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const
{
    out_text.clear();
    out_color = ff::color::none();
}

size_t ff::debug_pages_base::debug_page_toggle_count(size_t page) const
{
    return 0;
}

void ff::debug_pages_base::debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const
{
    out_text.clear();
    out_value = 0;
}

void ff::debug_pages_base::debug_page_toggle(size_t page, size_t index)
{}
