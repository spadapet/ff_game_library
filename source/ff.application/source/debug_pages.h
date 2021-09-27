#pragma once

namespace ff
{
    class debug_pages_base
    {
    public:
        virtual ~debug_pages_base() = default;

        virtual size_t debug_page_count() const;
        virtual std::string_view debug_page_name(size_t page) const;
        virtual void debug_page_update_stats(size_t page, bool update_fast_numbers);

        virtual size_t debug_page_info_count(size_t page) const;
        virtual void debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const;

        virtual size_t debug_page_toggle_count(size_t page) const;
        virtual void debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const;
        virtual void debug_page_toggle(size_t page, size_t index);
    };
}
