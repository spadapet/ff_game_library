#include "pch.h"
#include "assert.h"
#include "flags.h"
#include "log.h"
#include "thread_dispatch.h"
#include "window.h"

#if UWP_APP

namespace ff::internal
{
    class main_window_events
    {
    public:
        main_window_events(ff::window* main_window, winrt::Windows::UI::Core::CoreWindow core_window, winrt::Windows::Graphics::Display::DisplayInformation display_info)
            : main_window(main_window)
            , core_window(core_window)
            , display_info(display_info)
        {
            size_t i = 0;

            this->tokens[i++] = winrt::Windows::ApplicationModel::Core::CoreApplication::Suspending({ this, &main_window_events::app_suspending });
            this->tokens[i++] = winrt::Windows::ApplicationModel::Core::CoreApplication::Resuming({ this, &main_window_events::app_resuming });
            this->tokens[i++] = this->core_window.get().Activated({ this, &main_window_events::active_changed });
            this->tokens[i++] = this->core_window.get().VisibilityChanged({ this, &main_window_events::visiblity_changed });
            this->tokens[i++] = this->core_window.get().SizeChanged({ this, &main_window_events::size_changed });
            this->tokens[i++] = this->core_window.get().Closed({ this, &main_window_events::closed });
            this->tokens[i++] = this->core_window.get().CharacterReceived({ this, &main_window_events::character_received });
            this->tokens[i++] = this->core_window.get().KeyDown({ this, &main_window_events::key_down_or_up });
            this->tokens[i++] = this->core_window.get().KeyUp({ this, &main_window_events::key_down_or_up });
            this->tokens[i++] = this->core_window.get().PointerCaptureLost({ this, &main_window_events::pointer_capture_lost });
            this->tokens[i++] = this->core_window.get().PointerEntered({ this, &main_window_events::pointer_entered });
            this->tokens[i++] = this->core_window.get().PointerExited({ this, &main_window_events::pointer_exited });
            this->tokens[i++] = this->core_window.get().PointerMoved({ this, &main_window_events::pointer_moved });
            this->tokens[i++] = this->core_window.get().PointerPressed({ this, &main_window_events::pointer_pressed });
            this->tokens[i++] = this->core_window.get().PointerReleased({ this, &main_window_events::pointer_released });
            this->tokens[i++] = this->core_window.get().PointerWheelChanged({ this, &main_window_events::pointer_wheel_changed });
            this->tokens[i++] = this->core_window.get().Dispatcher().AcceleratorKeyActivated({ this, &main_window_events::accelerator_key_down });
            this->tokens[i++] = this->display_info.DpiChanged({ this, &main_window_events::dpi_changed });
            this->tokens[i++] = this->display_info.DisplayContentsInvalidated({ this, &main_window_events::display_invalidated });
            this->tokens[i++] = this->display_info.OrientationChanged({ this, &main_window_events::orientation_changed });
            this->tokens[i++] = winrt::Windows::Gaming::Input::Gamepad::GamepadAdded({ this, &main_window_events::gamepad_added });
            this->tokens[i++] = winrt::Windows::Gaming::Input::Gamepad::GamepadRemoved({ this, &main_window_events::gamepad_removed });

            assert(i == this->tokens.size());
        }

    public:
        virtual ~main_window_events()
        {
            size_t i = 0;

            winrt::Windows::ApplicationModel::Core::CoreApplication::Suspending(this->tokens[i++]);
            winrt::Windows::ApplicationModel::Core::CoreApplication::Resuming(this->tokens[i++]);
            this->core_window.get().Activated(this->tokens[i++]);
            this->core_window.get().VisibilityChanged(this->tokens[i++]);
            this->core_window.get().SizeChanged(this->tokens[i++]);
            this->core_window.get().Closed(this->tokens[i++]);
            this->core_window.get().CharacterReceived(this->tokens[i++]);
            this->core_window.get().KeyDown(this->tokens[i++]);
            this->core_window.get().KeyUp(this->tokens[i++]);
            this->core_window.get().PointerCaptureLost(this->tokens[i++]);
            this->core_window.get().PointerEntered(this->tokens[i++]);
            this->core_window.get().PointerExited(this->tokens[i++]);
            this->core_window.get().PointerMoved(this->tokens[i++]);
            this->core_window.get().PointerPressed(this->tokens[i++]);
            this->core_window.get().PointerReleased(this->tokens[i++]);
            this->core_window.get().PointerWheelChanged(this->tokens[i++]);
            this->core_window.get().Dispatcher().AcceleratorKeyActivated(this->tokens[i++]);
            this->display_info.DpiChanged(this->tokens[i++]);
            this->display_info.DisplayContentsInvalidated(this->tokens[i++]);
            this->display_info.OrientationChanged(this->tokens[i++]);
            winrt::Windows::Gaming::Input::Gamepad::GamepadAdded(this->tokens[i++]);
            winrt::Windows::Gaming::Input::Gamepad::GamepadRemoved(this->tokens[i++]);

            assert(i == this->tokens.size());
        }

    private:
        void app_suspending(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::ApplicationModel::SuspendingEventArgs& args)
        {
            winrt::Windows::ApplicationModel::SuspendingDeferral deferral = args.SuspendingOperation().GetDeferral();

            ff::thread_dispatch::get_main()->post([deferral, main_window = this->main_window]()
            {
                    ff::window_message msg{ WM_POWERBROADCAST, PBT_APMSUSPEND };
                    main_window->notify_message(msg);
                    deferral.Complete();
            });
        }

        void app_resuming(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& arg)
        {
            ff::window_message msg1{ WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC };
            ff::window_message msg2{ WM_POWERBROADCAST, PBT_APMRESUMESUSPEND };

            this->main_window->notify_message(msg1);
            this->main_window->notify_message(msg2);
        }

        void active_changed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::WindowActivatedEventArgs& args)
        {
            WPARAM active = args.WindowActivationState() != winrt::Windows::UI::Core::CoreWindowActivationState::Deactivated;
            UINT focus_message = active ? WM_SETFOCUS : WM_KILLFOCUS;

            ff::window_message msg1{ WM_ACTIVATEAPP, active };
            ff::window_message msg2{ WM_ACTIVATE, active };
            ff::window_message msg3{ focus_message };

            this->main_window->notify_message(msg1);
            this->main_window->notify_message(msg2);
            this->main_window->notify_message(msg3);
        }

        void visiblity_changed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::VisibilityChangedEventArgs& args)
        {
            WPARAM size_wp = args.Visible() ? SIZE_RESTORED : SIZE_MINIMIZED;
            ff::window_message msg1{ WM_SHOWWINDOW, args.Visible() };
            ff::window_message msg2{ WM_SIZE, size_wp };

            this->main_window->notify_message(msg1);
            this->main_window->notify_message(msg2);
        }

        void size_changed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::WindowSizeChangedEventArgs& args)
        {
            ff::window_message msg{ WM_SIZE, SIZE_RESTORED };
            this->main_window->notify_message(msg);
        }

        void closed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::CoreWindowEventArgs& args)
        {
            ff::window_message msg1{ WM_CLOSE };
            ff::window_message msg2{ WM_DESTROY };
            ff::window_message msg3{ WM_NCDESTROY };

            this->main_window->notify_message(msg1);
            this->main_window->notify_message(msg2);
            this->main_window->notify_message(msg3);
        }

        void character_received(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::CharacterReceivedEventArgs& args)
        {
            if (args.KeyCode() < 0x10000)
            {
                ff::window_message msg{ WM_CHAR, static_cast<WPARAM>(args.KeyCode()) };
                this->main_window->notify_message(msg);
            }
            else
            {
                unsigned int utf32 = args.KeyCode() - 0x10000;
                ff::window_message msg1{ WM_CHAR, static_cast<WPARAM>((utf32 / 0x400) + 0xd800) };
                ff::window_message msg2{ WM_CHAR, static_cast<WPARAM>((utf32 % 0x400) + 0xdc00) };

                this->main_window->notify_message(msg1);
                this->main_window->notify_message(msg2);
            }
        }

        void key_down_or_up(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::KeyEventArgs& args)
        {
            // accelerator_key_down should take care of every key
        }

        void accelerator_key_down(const winrt::Windows::UI::Core::CoreDispatcher& sender, const winrt::Windows::UI::Core::AcceleratorKeyEventArgs& args)
        {
            this->notify_key_message(args.VirtualKey(), args.KeyStatus(), args.EventType());
        }

        void notify_key_message(
            winrt::Windows::System::VirtualKey key,
            winrt::Windows::UI::Core::CorePhysicalKeyStatus status,
            winrt::Windows::UI::Core::CoreAcceleratorKeyEventType type)
        {
            WPARAM wp = static_cast<WPARAM>(key);
            LPARAM lp = static_cast<LPARAM>(status.RepeatCount);
            lp |= static_cast<LPARAM>(status.ScanCode & 0xFF) << 16;
            lp |= (status.IsExtendedKey ? 1 : 0) << 24;
            lp |= (status.IsMenuKeyDown ? 1 : 0) << 29;
            lp |= (status.WasKeyDown ? 1 : 0) << 30;

            UINT msg = 0;
            switch (type)
            {
                case winrt::Windows::UI::Core::CoreAcceleratorKeyEventType::KeyDown:
                    msg = WM_KEYDOWN;
                    break;

                case winrt::Windows::UI::Core::CoreAcceleratorKeyEventType::KeyUp:
                    msg = WM_KEYUP;
                    break;

                case winrt::Windows::UI::Core::CoreAcceleratorKeyEventType::SystemCharacter:
                    msg = WM_SYSCHAR;
                    break;

                case winrt::Windows::UI::Core::CoreAcceleratorKeyEventType::SystemKeyDown:
                    msg = WM_SYSKEYDOWN;
                    break;

                case winrt::Windows::UI::Core::CoreAcceleratorKeyEventType::SystemKeyUp:
                    msg = WM_SYSKEYUP;
                    break;
            }

            if (msg)
            {
                ff::window_message msg2{ msg, wp, lp };
                this->main_window->notify_message(msg2);
            }
        }

        void pointer_capture_lost(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERCAPTURECHANGED, args);
        }

        void pointer_entered(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERENTER, args);
        }

        void pointer_exited(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERLEAVE, args);
        }

        void pointer_moved(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERUPDATE, args);
        }

        void pointer_pressed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERDOWN, args);
        }

        void pointer_released(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERUP, args);
        }

        void pointer_wheel_changed(const winrt::Windows::UI::Core::CoreWindow& sender, const winrt::Windows::UI::Core::PointerEventArgs& args)
        {
            this->main_window->notify_pointer_message(WM_POINTERWHEEL, args);
        }

        void dpi_changed(const winrt::Windows::Graphics::Display::DisplayInformation& display_info, const winrt::Windows::Foundation::IInspectable& sender)
        {
            int dpi = static_cast<int>(display_info.LogicalDpi());
            ff::window_message msg{ WM_DPICHANGED, MAKEWPARAM(dpi, dpi) };
            this->main_window->notify_message(msg);
        }

        void display_invalidated(const winrt::Windows::Graphics::Display::DisplayInformation& display_info, const winrt::Windows::Foundation::IInspectable& sender)
        {
            ff::window_message msg{ WM_DISPLAYCHANGE };
            this->main_window->notify_message(msg);
        }

        void orientation_changed(const winrt::Windows::Graphics::Display::DisplayInformation& display_info, const winrt::Windows::Foundation::IInspectable& sender)
        {
            ff::window_message msg{ WM_DISPLAYCHANGE };
            this->main_window->notify_message(msg);
        }

        void gamepad_added(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Gaming::Input::Gamepad& gamepad)
        {
            this->main_window->notify_gamepad_message(true, gamepad);
        }

        void gamepad_removed(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Gaming::Input::Gamepad& gamepad)
        {
            this->main_window->notify_gamepad_message(false, gamepad);
        }

        ff::window* main_window;
        winrt::agile_ref<winrt::Windows::UI::Core::CoreWindow> core_window;
        winrt::Windows::Graphics::Display::DisplayInformation display_info;
        std::array<winrt::event_token, 22> tokens;
    };
}

static ff::window* main_window = nullptr;

ff::window::window(ff::window_type type)
    : core_window(winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread())
    , display_info_(winrt::Windows::Graphics::Display::DisplayInformation::GetForCurrentView())
    , application_view_(winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView())
    , window_events(std::make_unique<ff::internal::main_window_events>(this, this->core_window.get(), this->display_info_))
    , dpi_scale_(this->display_info_.LogicalDpi() / 96.0)
    , allow_swap_chain_panel_(true)
    , active_(this->core_window.get().ActivationMode() != winrt::Windows::UI::Core::CoreWindowActivationMode::Deactivated)
    , visible_(this->core_window.get().Visible())
{
    if (type == ff::window_type::main)
    {
        assert(!::main_window);
        ::main_window = this;
    }
}

ff::window::window(window&& other) noexcept
    : window(ff::window_type::none)
{
    *this = std::move(other);
}

ff::window::~window()
{
    if (this == ::main_window)
    {
        ::main_window = nullptr;
    }
}

ff::window& ff::window::operator=(window&& other) noexcept
{
    if (&other == ::main_window)
    {
        ::main_window = this;
    }

    return *this;
}

ff::window::operator bool() const
{
    return true;
}

bool ff::window::operator!() const
{
    return false;
}

bool ff::window::allow_swap_chain_panel()
{
    return this->allow_swap_chain_panel_;
}

void ff::window::allow_swap_chain_panel(bool value)
{
    this->allow_swap_chain_panel_ = value;
}

winrt::Windows::UI::Xaml::Controls::SwapChainPanel ff::window::swap_chain_panel() const
{
    if (this->allow_swap_chain_panel_)
    {
        winrt::Windows::UI::Xaml::Window window = winrt::Windows::UI::Xaml::Window::Current();
        if (window)
        {
            winrt::Windows::UI::Xaml::Controls::Page page = window.Content().as<winrt::Windows::UI::Xaml::Controls::Page>();
            if (page)
            {
                return page.Content().as<winrt::Windows::UI::Xaml::Controls::SwapChainPanel>();
            }

            return window.Content().as<winrt::Windows::UI::Xaml::Controls::SwapChainPanel>();
        }
    }

    return nullptr;
}

const winrt::Windows::Graphics::Display::DisplayInformation& ff::window::display_info() const
{
    return this->display_info_;
}

const winrt::Windows::UI::ViewManagement::ApplicationView& ff::window::application_view() const
{
    return this->application_view_;
}

ff::signal_sink<bool, const winrt::Windows::Gaming::Input::Gamepad&>& ff::window::gamepad_message_sink()
{
    return this->gamepad_message_signal;
}

void ff::window::notify_gamepad_message(bool added, const winrt::Windows::Gaming::Input::Gamepad& gamepad)
{
    this->gamepad_message_signal.notify(added, gamepad);
}

ff::signal_sink<unsigned int, const winrt::Windows::UI::Core::PointerEventArgs&>& ff::window::pointer_message_sink()
{
    return this->pointer_message_signal;
}

void ff::window::notify_pointer_message(unsigned int msg, const winrt::Windows::UI::Core::PointerEventArgs& args)
{
    this->pointer_message_signal.notify(msg, args);
}

ff::window::handle_type ff::window::handle() const
{
    return this->core_window.get();
}

ff::window::operator handle_type() const
{
    return this->handle();
}

bool ff::window::operator==(handle_type handle) const
{
    return this->handle() == handle;
}

ff::window* ff::window::main()
{
    assert(::main_window);
    return ::main_window;
}

ff::signal_sink<ff::window_message&>& ff::window::message_sink()
{
    return this->message_signal;
}

void ff::window::notify_message(ff::window_message& message)
{
    switch (message.msg)
    {
        case WM_ACTIVATE:
            this->active_ = message.wp != 0;
            break;

        case WM_SHOWWINDOW:
            this->visible_ = message.wp != 0;
            break;

        case WM_DPICHANGED:
            this->dpi_scale_ = this->display_info_.LogicalDpi() / 96.0;
            break;
    }

    this->message_signal.notify(message);
}

bool ff::window::key_down(int vk) const
{
    auto state = this->core_window.get().GetKeyState(static_cast<winrt::Windows::System::VirtualKey>(vk));
    return ff::flags::has(state, winrt::Windows::UI::Core::CoreVirtualKeyStates::Down);
}

// https://docs.microsoft.com/en-us/uwp/api/windows.graphics.display.displayorientations?view=winrt-22000
static int get_rotation(
    winrt::Windows::Graphics::Display::DisplayOrientations native,
    winrt::Windows::Graphics::Display::DisplayOrientations current)
{
    switch (native)
    {
        default:
            switch (current)
            {
                default:
                    return DMDO_DEFAULT;

                case winrt::Windows::Graphics::Display::DisplayOrientations::Portrait:
                    return DMDO_90;

                case winrt::Windows::Graphics::Display::DisplayOrientations::LandscapeFlipped:
                    return DMDO_180;

                case winrt::Windows::Graphics::Display::DisplayOrientations::PortraitFlipped:
                    return DMDO_270;
            }
            break;

        case winrt::Windows::Graphics::Display::DisplayOrientations::Portrait:
            switch (current)
            {
                default:
                    return DMDO_DEFAULT;

                case winrt::Windows::Graphics::Display::DisplayOrientations::LandscapeFlipped:
                    return DMDO_90;

                case winrt::Windows::Graphics::Display::DisplayOrientations::PortraitFlipped:
                    return DMDO_180;

                case winrt::Windows::Graphics::Display::DisplayOrientations::Landscape:
                    return DMDO_270;
            }
            break;
    }
}

ff::window_size ff::window::size()
{
    ff::window_size size{};

    ff::thread_dispatch::get_main()->send([this, &size]()
    {
        winrt::Windows::Foundation::Rect bounds = this->core_window.get().Bounds();
        size.dpi_scale = this->dpi_scale();
        size.logical_pixel_size = (ff::point_double(bounds.Width, bounds.Height) * size.dpi_scale).cast<size_t>();
        size.rotation = ::get_rotation(this->display_info_.NativeOrientation(), this->display_info_.CurrentOrientation());
    });

    return size;
}

double ff::window::dpi_scale()
{
    return this->dpi_scale_;
}

bool ff::window::active()
{
    return this->active_;
}

bool ff::window::visible()
{
    return this->visible_;
}

bool ff::window::enabled()
{
    return true;
}

bool ff::window::focused()
{
    return this->active();
}

bool ff::window::close()
{
    this->application_view_.TryConsolidateAsync();
    return true;
}

#endif
