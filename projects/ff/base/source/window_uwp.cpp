#include "pch.h"
#include "window.h"

#if UWP_APP

static ff::window* main_window = nullptr;

ref class main_window_events sealed
{
public:
    main_window_events(Windows::UI::Core::CoreWindow^ core_window, Windows::Graphics::Display::DisplayInformation^ display_info)
        : core_window(core_window)
        , display_info(display_info)
    {
        size_t i = 0;

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

        this->tokens[i++] = this->display_info->DpiChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::dpi_changed);

        this->tokens[i++] = this->display_info->DisplayContentsInvalidated +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::display_invalidated);

        this->tokens[i++] = this->display_info->OrientationChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::orientation_changed);
    }

    virtual ~main_window_events()
    {
        size_t i = 0;

        this->core_window->Activated -= this->tokens[i++];
        this->core_window->VisibilityChanged -= this->tokens[i++];
        this->core_window->SizeChanged -= this->tokens[i++];
        this->core_window->Closed -= this->tokens[i++];
        this->core_window->CharacterReceived -= this->tokens[i++];
        this->core_window->KeyDown -= this->tokens[i++];
        this->core_window->KeyUp -= this->tokens[i++];
        this->display_info->DpiChanged -= this->tokens[i++];
        this->display_info->DisplayContentsInvalidated -= this->tokens[i++];
        this->display_info->OrientationChanged -= this->tokens[i++];
    }

private:
    void active_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowActivatedEventArgs^ args)
    {
        WPARAM active = args->WindowActivationState != Windows::UI::Core::CoreWindowActivationState::Deactivated;
        ff::window::main()->send_message(WM_ACTIVATEAPP, active, 0);
        ff::window::main()->send_message(active ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    }

    void visiblity_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
    {
        ff::window::main()->send_message(WM_SHOWWINDOW, args->Visible, 0);
        ff::window::main()->send_message(WM_SIZE, args->Visible ? SIZE_RESTORED : SIZE_MINIMIZED, 0);
    }

    void size_changed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
    {
        ff::window::main()->send_message(WM_SIZE, SIZE_RESTORED, 0);
    }

    void closed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
    {
        ff::window::main()->send_message(WM_DESTROY, 0, 0);
    }

    void character_received(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CharacterReceivedEventArgs^ args)
    {
        if (args->KeyCode < 0x10000)
        {
            ff::window::main()->send_message(WM_CHAR, static_cast<WPARAM>(args->KeyCode), 0);
        }
        else
        {
            unsigned int utf32 = args->KeyCode - 0x10000;
            ff::window::main()->send_message(WM_CHAR, static_cast<WPARAM>((utf32 / 0x400) + 0xd800), 0);
            ff::window::main()->send_message(WM_CHAR, static_cast<WPARAM>((utf32 % 0x400) + 0xdc00), 0);
        }
    }

    void key_down_or_up(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args)
    {
        WPARAM wp = static_cast<WPARAM>(args->VirtualKey);
        LPARAM lp = static_cast<LPARAM>(args->KeyStatus.RepeatCount);
        lp |= static_cast<LPARAM>(args->KeyStatus.ScanCode & 0xFF) << 16;
        lp |= (args->KeyStatus.IsExtendedKey ? 1 : 0) << 24;
        lp |= (args->KeyStatus.WasKeyDown ? 1 : 0) << 30;

        ff::window::main()->send_message(args->KeyStatus.IsKeyReleased ? WM_KEYUP : WM_KEYDOWN, wp, lp);
    }

    void dpi_changed(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        ff::window::main()->send_message(WM_DPICHANGED, 0, 0);
    }

    void display_invalidated(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        ff::window::main()->send_message(WM_DISPLAYCHANGE, 0, 0);
    }

    void orientation_changed(Windows::Graphics::Display::DisplayInformation^ display_info, Platform::Object^ sender)
    {
        ff::window::main()->send_message(WM_DISPLAYCHANGE, 0, 0);
    }

    Platform::Agile<Windows::UI::Core::CoreWindow> core_window;
    Windows::Graphics::Display::DisplayInformation^ display_info;
    Windows::Foundation::EventRegistrationToken tokens[10];
};

Windows::UI::Xaml::Controls::SwapChainPanel^ ff::window::swap_chain_panel()
{
    Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
    Windows::UI::Xaml::Controls::Page^ page = dynamic_cast<Windows::UI::Xaml::Controls::Page^>(window->Content);
    return dynamic_cast<Windows::UI::Xaml::Controls::SwapChainPanel^>(page ? page->Content : window->Content);
}

void ff::window::send_message(UINT msg, WPARAM wp, LPARAM lp)
{
    ff::window_message message{ nullptr, msg, wp, lp, 0, false };
    this->message_signal.notify(message);
}

ff::window::window()
    : core_window(Windows::UI::Core::CoreWindow::GetForCurrentThread())
    , display_info(Windows::Graphics::Display::DisplayInformation::GetForCurrentView())
    , window_events(ref new main_window_events(this->core_window.Get(), this->display_info))
{
    assert(!::main_window);
    ::main_window = this;
}

ff::window::~window()
{
    if (this == ::main_window)
    {
        ::main_window = nullptr;
    }
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

static int get_rotation(Windows::Graphics::Display::DisplayOrientations orientation)
{
    switch (orientation)
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
}

ff::window_size ff::window::size()
{
    Windows::Foundation::Rect bounds = this->core_window->Bounds;

    ff::window_size size{};
    size.dpi_scale = this->dpi_scale();
    size.pixel_size = (ff::point_double(bounds.Width, bounds.Height) * size.dpi_scale).cast<int>();
    size.native_rotation = ::get_rotation(this->display_info->NativeOrientation);
    size.current_rotation = ::get_rotation(this->display_info->CurrentOrientation);

    return size;
}

double ff::window::dpi_scale()
{
    return this->display_info->LogicalDpi;
}

bool ff::window::active()
{
    return this->core_window->ActivationMode != Windows::UI::Core::CoreWindowActivationMode::Deactivated;
}

bool ff::window::visible()
{
    return this->core_window->Visible;
}

bool ff::window::focused()
{
    return this->active();
}

bool ff::window::close()
{
    Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->TryConsolidateAsync();
    return true;
}

#endif
