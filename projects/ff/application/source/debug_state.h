#pragma once

#include "debug_pages.h"
#include "state.h"

namespace ff
{
    class debug_state : public ff::state, public ff::debug_pages_base
    {
    public:
        debug_state();
        debug_state(debug_state&& other) noexcept = delete;
        debug_state(const debug_state& other) = delete;
        virtual ~debug_state() override;

        debug_state& operator=(debug_state&& other) noexcept = delete;
        debug_state& operator=(const debug_state& other) = delete;

        // state
        virtual std::shared_ptr<ff::state> advance_time() override;
        virtual void advance_input() override;
        virtual void render(ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual void frame_rendered(ff::state::advance_t type, ff::dx11_target_base& target, ff::dx11_depth& depth) override;
        virtual ff::state::status_t status() override;

        // debug_pages_base
        virtual size_t debug_page_count() const override;
        virtual std::string_view debug_page_name(size_t page) const override;
        virtual void debug_page_update_stats(size_t page, bool update_fast_numbers) override;
        virtual size_t debug_page_info_count(size_t page) const override;
        virtual void debug_page_info(size_t page, size_t index, std::string& out_text, DirectX::XMFLOAT4& out_color) const override;
        virtual size_t debug_page_toggle_count(size_t page) const override;
        virtual void debug_page_toggle_info(size_t page, size_t index, std::string& out_text, int& out_value) const override;
        virtual void debug_page_toggle(size_t page, size_t index) override;

    private:
        std::unique_ptr<ff::draw_device> draw_device;
    };

    void add_debug_pages(ff::debug_pages_base* pages);
    void remove_debug_pages(ff::debug_pages_base* pages);
    ff::signal_sink<void()>& custom_debug_sink();
}
