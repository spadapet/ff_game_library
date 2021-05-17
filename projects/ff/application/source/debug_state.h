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
        void update_stats();
        void render_text(ff::dx11_target_base& target, ff::dx11_depth& depth);
        void render_charts(ff::dx11_target_base& target);
        void toggle(size_t index);
        size_t total_page_count() const;
        ff::debug_pages_base* convert_page_to_sub_page(size_t debugPage, size_t& outPage, size_t& outSubPage) const;

        static constexpr size_t MAX_QUEUE_SIZE = ff::constants::advances_per_second_s * 6; // six seconds of data

        bool enabled_stats;
        bool enabled_charts;
        size_t debug_page;
        ff::auto_resource<ff::sprite_font> font;
        ff::auto_resource<ff::input_mapping> input_mapping;
        std::unique_ptr<ff::input_event_provider> input_events;
        std::unique_ptr<ff::draw_device> draw_device;

        ff::memory::allocation_stats mem_stats;
        ff::graphics_counters graphics_counters;
        size_t total_advance_count;
        size_t total_render_count;
        size_t advance_count;
        size_t fast_number_counter;
        size_t aps_counter;
        size_t rps_counter;
        double last_aps;
        double last_rps;
        double total_seconds;
        double old_seconds;
        double advance_time_total;
        double advance_time_average;
        double render_time;
        double flip_time;
        double bank_time;

        struct frame_t
        {
            float advance_time;
            float render_time;
            float total_time;
        };

        std::array<ff::debug_state::frame_t, MAX_QUEUE_SIZE> frames;
        size_t frames_end;
    };

    void add_debug_pages(ff::debug_pages_base* pages);
    void remove_debug_pages(ff::debug_pages_base* pages);
    ff::signal_sink<>& custom_debug_sink();
}
