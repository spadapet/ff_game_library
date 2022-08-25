#include "pch.h"
#include "assert.h"
#include "log.h"
#include "thread_dispatch.h"
#include "window.h"

#if UWP_APP

ref class main_window_events sealed
{
internal:
    main_window_events(ff::window* main_window, Windows::UI::Core::CoreWindow^ core_window, Windows::Graphics::Display::DisplayInformation^ display_info)
        : main_window(main_window)
        , core_window(core_window)
        , display_info(display_info)
    {
        size_t i = 0;

        this->tokens[i++] = Windows::ApplicationModel::Core::CoreApplication::Suspending +=
            ref new Windows::Foundation::EventHandler<Windows::ApplicationModel::SuspendingEventArgs^>(
                this, &main_window_events::app_suspending);

        this->tokens[i++] = Windows::ApplicationModel::Core::CoreApplication::Resuming +=
            ref new Windows::Foundation::EventHandler<Platform::Object^>(
                this, &main_window_events::app_resuming);

        this->tokens[i++] = this->core_window->Activated +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::WindowActivatedEventArgs^>(
                this, &main_window_events::active_changed);

        this->tokens[i++] = this->core_window->VisibilityChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::VisibilityChangedEventArgs^>(
                this, &main_window_events::visiblity_changed);

        this->tokens[i++] = this->core_window->SizeChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::WindowSizeChangedEventArgs^>(
                this, &main_window_events::size_changed);

        this->tokens[i++] = this->core_window->Closed +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::CoreWindowEventArgs^>(
                this, &main_window_events::closed);

        this->tokens[i++] = this->core_window->CharacterReceived +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::CharacterReceivedEventArgs^>(
                this, &main_window_events::character_received);

        this->tokens[i++] = this->core_window->KeyDown +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(
                this, &main_window_events::key_down_or_up);

        this->tokens[i++] = this->core_window->KeyUp +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::KeyEventArgs^>(
                this, &main_window_events::key_down_or_up);

        this->tokens[i++] = this->core_window->PointerCaptureLost +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_capture_lost);

        this->tokens[i++] = this->core_window->PointerEntered +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_entered);

        this->tokens[i++] = this->core_window->PointerExited +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_exited);

        this->tokens[i++] = this->core_window->PointerMoved +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_moved);

        this->tokens[i++] = this->core_window->PointerPressed +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_pressed);

        this->tokens[i++] = this->core_window->PointerReleased +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_released);

        this->tokens[i++] = this->core_window->PointerWheelChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreWindow^, Windows::UI::Core::PointerEventArgs^>(
                this, &main_window_events::pointer_wheel_changed);

        this->tokens[i++] = this->core_window->Dispatcher->AcceleratorKeyActivated +=
            ref new Windows::Foundation::TypedEventHandler<Windows::UI::Core::CoreDispatcher^, Windows::UI::Core::AcceleratorKeyEventArgs^>(
                this, &main_window_events::accelerator_key_down);

        this->tokens[i++] = this->display_info->DpiChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::dpi_changed);

        this->tokens[i++] = this->display_info->DisplayContentsInvalidated +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::display_invalidated);

        this->tokens[i++] = this->display_info->OrientationChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::orientation_changed);

        this->tokens[i++] = Windows::Gaming::Input::Gamepad::GamepadAdded +=
            ref new Windows::Foundation::EventHandler<Windows::Gaming::Input::Gamepad^>(
                this, &main_window_events::gamepad_added);

        this->tokens[i++] = Windows::Gaming::Input::Gamepad::GamepadRemoved +=
            ref new Windows::Foundation::EventHandler<Windows::Gaming::Input::Gamepad^>(
                this, &main_window_events::gamepad_removed);

        assert(i == this->tokens.size());
    }

public:
    virtual ~main_window_events()
    {
        size_t i = 0;

        Windows::ApplicationModel::Core::CoreApplication::Suspending -= this->tokens[i++];
        Windows::ApplicationModel::Core::CoreApplication::Resuming -= this->tokens[i++];
        this->core_window->Activated -= this->tokens[i++];
        this->core_window->VisibilityChanged -= this->tokens[i++];
        this->core_window->SizeChanged -= this->tokens[i++];
        this->core_window->Closed -= this->tokens[i++];
        this->core_window->CharacterReceived -= this->tokens[i++];
        this->core_window->KeyDown -= this->tokens[i++];
        this->core_window->KeyUp -= this->tokens[i++];
        this->core_window->PointerCaptureLost -= this->tokens[i++];
        this->core_window->PointerEntered -= this->tokens[i++];
        this->core_window->PointerExited -= this->tokens[i++];
        this->core_window->PointerMoved -= this->tokens[i++];
        this->core_window->PointerPressed -= this->tokens[i++];
        this->core_window->PointerReleased -= this->tokens[i++];
        this->core_window->PointerWheelChanged -= this->tokens[i++];
        this->core_window->Dispatcher->AcceleratorKeyActivated -= this->tokens[i++];
        this->display_info->DpiChanged -= this->tokens[i++];
        this->display_info->DisplayContentsInvalidated -= this->tokens[i++];
        this->display_info->OrientationChanged -= this->tokens[i++];
        Windows::Gaming::Input::Gamepad::GamepadAdded -= this->tokens[i++];
        Windows::Gaming::Input::Gamepad::GamepadRemoved -= this->tokens[i++];

        assert(i == this->tokens.size());
    }

private:
    void app_suspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args)
    {
        Windows::ApplicationModel::SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

        ff::thread_dispatch::get_main()->post([deferral, main_window = this->main_window]()
        {
            ff::window_message msg{ WM_POWERBROADCAST, PBT_APMSUSPEND };
            main_window->notify_message(msg);
            deferral->Complete();
        });
    }

    void app_resuming(Platform::Object^ sender, Platform::Object^ arg)
    {
        ff::window_message msg1{ WM_POWERBROADCAST, PBT_APMRESUMEAUTOMATIC };
        ff::window_message msg2{ WM_POWERBROADCAST, PBT_APMRESUMESUSPEND };

        this->main_window->notify_message(msg1);
        this->main_window->notify_message(msg2);
    }

    void active_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowActivatedEventArgs^ args)
    {
        WPARAM active = args->WindowActivationState != Windows::UI::Core::CoreWindowActivationState::Deactivated;
        UINT focus_message = active ? WM_SETFOCUS : WM_KILLFOCUS;

        ff::window_message msg1{ WM_ACTIVATEAPP, active };
        ff::window_message msg2{ WM_ACTIVATE, active };
        ff::window_message msg3{ focus_message };

        this->main_window->notify_message(msg1);
        this->main_window->notify_message(msg2);
        this->main_window->notify_message(msg3);
    }

    void visiblity_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
    {
        WPARAM size_wp = args->Visible ? SIZE_RESTORED : SIZE_MINIMIZED;
        ff::window_message msg1{ WM_SHOWWINDOW, args->Visible };
        ff::window_message msg2{ WM_SIZE, size_wp };

        this->main_window->notify_message(msg1);
        this->main_window->notify_message(msg2);
    }

    void size_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
    {
        ff::window_message msg{ WM_SIZE, SIZE_RESTORED };
        this->main_window->notify_message(msg);
    }

    void closed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
    {
        ff::window_message msg1{ WM_CLOSE };
        ff::window_message msg2{ WM_DESTROY };

        this->main_window->notify_message(msg1);
        this->main_window->notify_message(msg2);
    }

    void character_received(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args)
    {
        if (args->KeyCode < 0x10000)
        {
            ff::window_message msg{ WM_CHAR, static_cast<WPARAM>(args->KeyCode) };
            this->main_window->notify_message(msg);
        }
        else
        {
            unsigned int utf32 = args->KeyCode - 0x10000;
            ff::window_message msg1{ WM_CHAR, static_cast<WPARAM>((utf32 / 0x400) + 0xd800) };
            ff::window_message msg2{ WM_CHAR, static_cast<WPARAM>((utf32 % 0x400) + 0xdc00) };

            this->main_window->notify_message(msg1);
            this->main_window->notify_message(msg2);
        }
    }

    void key_down_or_up(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
    {
        // accelerator_key_down should take care of every key
    }

    void accelerator_key_down(Windows::UI::Core::CoreDispatcher^ sender, Windows::UI::Core::AcceleratorKeyEventArgs^ args)
    {
        this->notify_key_message(args->VirtualKey, args->KeyStatus, args->EventType);
    }

    void notify_key_message(
        Windows::System::VirtualKey key,
        Windows::UI::Core::CorePhysicalKeyStatus status,
        Windows::UI::Core::CoreAcceleratorKeyEventType type)
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
            case Windows::UI::Core::CoreAcceleratorKeyEventType::KeyDown:
                msg = WM_KEYDOWN;
                break;

            case Windows::UI::Core::CoreAcceleratorKeyEventType::KeyUp:
                msg = WM_KEYUP;
                break;

            case Windows::UI::Core::CoreAcceleratorKeyEventType::SystemCharacter:
                msg = WM_SYSCHAR;
                break;

            case Windows::UI::Core::CoreAcceleratorKeyEventType::SystemKeyDown:
                msg = WM_SYSKEYDOWN;
                break;

            case Windows::UI::Core::CoreAcceleratorKeyEventType::SystemKeyUp:
                msg = WM_SYSKEYUP;
                break;
        }

        if (msg)
        {
            ff::window_message msg2{ msg, wp, lp };
            this->main_window->notify_message(msg2);
        }
    }

    void pointer_capture_lost(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERCAPTURECHANGED, args);
    }

    void pointer_entered(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERENTER, args);
    }

    void pointer_exited(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERLEAVE, args);
    }

    void pointer_moved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERUPDATE, args);
    }

    void pointer_pressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERDOWN, args);
    }

    void pointer_released(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERUP, args);
    }

    void pointer_wheel_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
    {
        this->main_window->notify_pointer_message(WM_POINTERWHEEL, args);
    }

    void dpi_changed(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        int dpi = static_cast<int>(display_info->LogicalDpi);
        ff::window_message msg{ WM_DPICHANGED, MAKEWPARAM(dpi, dpi) };
        this->main_window->notify_message(msg);
    }

    void display_invalidated(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        ff::window_message msg{ WM_DISPLAYCHANGE };
        this->main_window->notify_message(msg);
    }

    void orientation_changed(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        ff::window_message msg{ WM_DISPLAYCHANGE };
        this->main_window->notify_message(msg);
    }

    void gamepad_added(Platform::Object^ sender, Windows::Gaming::Input::Gamepad^ gamepad)
    {
        this->main_window->notify_gamepad_message(true, gamepad);
    }

    void gamepad_removed(Platform::Object^ sender, Windows::Gaming::Input::Gamepad^ gamepad)
    {
        this->main_window->notify_gamepad_message(false, gamepad);
    }

    ff::window* main_window;
    Platform::Agile<Windows::UI::Core::CoreWindow> core_window;
    Windows::Graphics::Display::DisplayInformation^ display_info;
    std::array<Windows::Foundation::EventRegistrationToken, 22> tokens;
};

static ff::window* main_window = nullptr;

ff::window::window(ff::window_type type)
    : core_window(Windows::UI::Core::CoreWindow::GetForCurrentThread())
    , display_info_(Windows::Graphics::Display::DisplayInformation::GetForCurrentView())
    , application_view_(Windows::UI::ViewManagement::ApplicationView::GetForCurrentView())
    , window_events(ref new main_window_events(this, this->core_window.Get(), this->display_info_))
    , dpi_scale_(this->display_info_->LogicalDpi / 96.0)
    , allow_swap_chain_panel_(true)
    , active_(this->core_window->ActivationMode != Windows::UI::Core::CoreWindowActivationMode::Deactivated)
    , visible_(this->core_window->Visible)
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

Windows::UI::Xaml::Controls::SwapChainPanel^ ff::window::swap_chain_panel() const
{
    if (this->allow_swap_chain_panel_)
    {
        Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
        if (window)
        {
            Windows::UI::Xaml::Controls::Page^ page = dynamic_cast<Windows::UI::Xaml::Controls::Page^>(window->Content);
            if (page)
            {
                return dynamic_cast<Windows::UI::Xaml::Controls::SwapChainPanel^>(page->Content);
            }

            return dynamic_cast<Windows::UI::Xaml::Controls::SwapChainPanel^>(window->Content);
        }
    }

    return nullptr;
}

Windows::Graphics::Display::DisplayInformation^ ff::window::display_info() const
{
    return this->display_info_;
}

Windows::UI::ViewManagement::ApplicationView^ ff::window::application_view() const
{
    return this->application_view_;
}

ff::signal_sink<bool, Windows::Gaming::Input::Gamepad^>& ff::window::gamepad_message_sink()
{
    return this->gamepad_message_signal;
}

void ff::window::notify_gamepad_message(bool added, Windows::Gaming::Input::Gamepad^ gamepad)
{
    this->gamepad_message_signal.notify(added, gamepad);
}

ff::signal_sink<unsigned int, Windows::UI::Core::PointerEventArgs^>& ff::window::pointer_message_sink()
{
    return this->pointer_message_signal;
}

void ff::window::notify_pointer_message(unsigned int msg, Windows::UI::Core::PointerEventArgs^ args)
{
    this->pointer_message_signal.notify(msg, args);
}

ff::window::handle_type ff::window::handle() const
{
    return this->core_window.Get();
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
            this->dpi_scale_ = this->display_info_->LogicalDpi / 96.0;
            break;
    }

    this->message_signal.notify(message);
}

// https://docs.microsoft.com/en-us/uwp/api/windows.graphics.display.displayorientations?view=winrt-22000
static int get_rotation(Windows::Graphics::Display::DisplayOrientations native, Windows::Graphics::Display::DisplayOrientations current)
{
    switch (native)
    {
        default:
            switch (current)
            {
                default:
                    return DMDO_DEFAULT;

                case Windows::Graphics::Display::DisplayOrientations::Portrait:
                    return DMDO_90;

                case Windows::Graphics::Display::DisplayOrientations::LandscapeFlipped:
                    return DMDO_180;

                case Windows::Graphics::Display::DisplayOrientations::PortraitFlipped:
                    return DMDO_270;
            }
            break;

        case Windows::Graphics::Display::DisplayOrientations::Portrait:
            switch (current)
            {
                default:
                    return DMDO_DEFAULT;

                case Windows::Graphics::Display::DisplayOrientations::LandscapeFlipped:
                    return DMDO_90;

                case Windows::Graphics::Display::DisplayOrientations::PortraitFlipped:
                    return DMDO_180;

                case Windows::Graphics::Display::DisplayOrientations::Landscape:
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
        Windows::Foundation::Rect bounds = this->core_window->Bounds;
        size.dpi_scale = this->dpi_scale();
        size.logical_pixel_size = (ff::point_double(bounds.Width, bounds.Height) * size.dpi_scale).cast<size_t>();
        size.rotation = ::get_rotation(this->display_info_->NativeOrientation, this->display_info_->CurrentOrientation);
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

bool ff::window::focused()
{
    return this->active();
}

bool ff::window::close()
{
    this->application_view_->TryConsolidateAsync();
    return true;
}

#endif
