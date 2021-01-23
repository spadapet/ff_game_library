#pragma once

#include "input_device_base.h"
#include "input_device.h"

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
        pointer_device();
        virtual ~pointer_device() override;

        bool in_window() const;
        ff::point_double pos() const; // in pixels
        ff::point_double relative_pos() const;

        int release_count(int vk_button) const;
        int double_click_count(int vk_button) const;
        ff::point_double wheel_scroll() const;

        size_t touch_info_count() const;
        const pointer_touch_info& touch_info(size_t index) const;

        // input_vk
        virtual bool pressing(int vk_button) const override;
        virtual int press_count(int vk_button) const override;

        // input_device_base
        virtual void advance() override;
        virtual void kill_pending() override;
        virtual bool connected() const override;
        virtual ff::signal_sink<const ff::input_device_event&>& event_sink() override;
        virtual void notify_main_window_message(ff::window_message& message) override;
#if UWP_APP
        void notify_main_window_pointer_message(unsigned int msg, Windows::UI::Core::PointerEventArgs^ args);
#endif

    private:
        struct mouse_info
        {
            static const size_t BUTTON_COUNT = 7;

            ff::point_double pos;
            ff::point_double pos_relative;
            ff::point_double wheel_scroll;
            bool pressing[BUTTON_COUNT];
            uint8_t presses[BUTTON_COUNT];
            uint8_t releases[BUTTON_COUNT];
            uint8_t double_clicks[BUTTON_COUNT];
            bool inside_window;
        };

        struct internal_touch_info
        {
            internal_touch_info();

#if UWP_APP
            Windows::UI::Input::PointerPoint^ point;
#endif
            ff::pointer_touch_info info;
        };

#if UWP_APP
        ff::input_device_event mouse_moved(Windows::UI::Input::PointerPoint^ point);
        ff::input_device_event mouse_pressed(Windows::UI::Input::PointerPoint^ point);
        ff::input_device_event touch_moved(Windows::UI::Input::PointerPoint^ point);
        ff::input_device_event touch_pressed(Windows::UI::Input::PointerPoint^ point);
        ff::input_device_event touch_released(Windows::UI::Input::PointerPoint^ point);

        std::vector<internal_touch_info>::iterator find_touch_info(Windows::UI::Input::PointerPoint^ point, bool allow_create);
        void update_touch_info(internal_touch_info& info, Windows::UI::Input::PointerPoint^ point);
#else
        void mouse_message(const ff::window_message& message);
        void pointer_message(const ff::window_message& message);

        std::vector<internal_touch_info>::iterator find_touch_info(const ff::window_message& message, bool allow_create);
        void update_touch_info(internal_touch_info& info, const ff::window_message& message);
#endif

        std::mutex mutex;
        mouse_info mouse;
        mouse_info pending_mouse;
        std::vector<internal_touch_info> touches;
        std::vector<internal_touch_info> pending_touches;
        ff::signal<const ff::input_device_event&> device_event;
    };
}
