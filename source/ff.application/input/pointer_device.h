#pragma once

#include "../input/input_device_base.h"
#include "../input/input_device_event.h"

namespace ff
{
    struct pointer_touch_info
    {
        ff::input_device type;
        ff::point_double start_pos;
        ff::point_double pos;
        unsigned int id;
        unsigned int counter;
        unsigned int vk;
    };

    class pointer_device : public input_device_base
    {
    public:
        bool in_window() const;
        ff::point_double pos() const; // in pixels
        ff::point_double relative_pos() const;

        int release_count(int vk_button) const;
        int double_click_count(int vk_button) const;
        ff::point_double wheel_scroll() const;
        bool touch_to_mouse() const;
        void touch_to_mouse(bool value);

        size_t touch_info_count() const;
        const pointer_touch_info& touch_info(size_t index) const;

        // input_vk
        virtual bool pressing(int vk_button) const override;
        virtual int press_count(int vk_button) const override;

        // input_device_base
        virtual void update() override;
        virtual void kill_pending() override;
        virtual void notify_window_message(ff::window* window, ff::window_message& message) override;

    private:
        struct mouse_info
        {
            static constexpr size_t BUTTON_COUNT = 7;

            ff::point_double pos;
            ff::point_double pos_relative;
            ff::point_double wheel_scroll;
            bool pressing[BUTTON_COUNT];
            uint8_t press_count[BUTTON_COUNT];
            uint8_t release_count[BUTTON_COUNT];
            uint8_t double_clicks[BUTTON_COUNT];
            bool inside_window;
        };

        struct internal_touch_info
        {
            internal_touch_info();

            ff::pointer_touch_info info;
        };

        void mouse_message(const ff::window_message& message);
        void pointer_message(const ff::window_message& message);

        std::vector<internal_touch_info>::iterator find_touch_info(const ff::window_message& message, bool allow_create);
        void update_touch_info(internal_touch_info& info, const ff::window_message& message);

        std::mutex mutex;
        mouse_info mouse{};
        mouse_info pending_mouse{};
        std::vector<internal_touch_info> touches;
        std::vector<internal_touch_info> pending_touches;
        bool touch_to_mouse_{};
    };
}
