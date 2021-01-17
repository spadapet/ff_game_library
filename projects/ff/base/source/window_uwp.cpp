#include "pch.h"
#include "window.h"

#if UWP_APP

static ff::window* main_window = nullptr;

ref class main_window_events sealed
{
public:
    main_window_events()
    {
        Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
        Windows::Graphics::Display::DisplayInformation^ display_info = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        size_t i = 0;

        this->tokens[i++] = window->Activated +=
            ref new Windows::UI::Xaml::WindowActivatedEventHandler(this, &main_window_events::active_changed);

        this->tokens[i++] = window->VisibilityChanged +=
            ref new Windows::UI::Xaml::WindowVisibilityChangedEventHandler(this, &main_window_events::visiblity_changed);

        this->tokens[i++] = window->SizeChanged +=
            ref new Windows::UI::Xaml::WindowSizeChangedEventHandler(this, &main_window_events::size_changed);

        this->tokens[i++] = window->Closed +=
            ref new Windows::UI::Xaml::WindowClosedEventHandler(this, &main_window_events::closed);

        this->tokens[i++] = display_info->DpiChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::dpi_changed);

        this->tokens[i++] = display_info->DisplayContentsInvalidated +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::display_invalidated);

        this->tokens[i++] = display_info->OrientationChanged +=
            ref new Windows::Foundation::TypedEventHandler<Windows::Graphics::Display::DisplayInformation^, Platform::Object^>(
                this, &main_window_events::orientation_changed);
    }

    virtual ~main_window_events()
    {
        Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
        Windows::Graphics::Display::DisplayInformation^ display_info = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
        size_t i = 0;

        window->Activated -= this->tokens[i++];
        window->VisibilityChanged -= this->tokens[i++];
        window->SizeChanged -= this->tokens[i++];
        window->Closed -= this->tokens[i++];
        display_info->DpiChanged -= this->tokens[i++];
        display_info->DisplayContentsInvalidated -= this->tokens[i++];
        display_info->OrientationChanged -= this->tokens[i++];
    }

private:
    void active_changed(Platform::Object^ sender, Windows::UI::Core::WindowActivatedEventArgs^ args)
    {
        WPARAM active = args->WindowActivationState != Windows::UI::Core::CoreWindowActivationState::Deactivated;
        ff::window::main()->send_message(WM_ACTIVATEAPP, active, 0);
        ff::window::main()->send_message(active ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    }

    void visiblity_changed(Platform::Object^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args)
    {
        ff::window::main()->send_message(WM_SHOWWINDOW, args->Visible, 0);
        ff::window::main()->send_message(WM_SIZE, args->Visible ? SIZE_RESTORED : SIZE_MINIMIZED, 0);
    }

    void size_changed(Platform::Object^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
    {
        ff::window::main()->send_message(WM_SIZE, SIZE_RESTORED, 0);
    }

    void closed(Platform::Object^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
    {
        ff::window::main()->send_message(WM_DESTROY, 0, 0);
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

    Windows::Foundation::EventRegistrationToken tokens[7];
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
    : window_events(ref new main_window_events())
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
    Windows::Graphics::Display::DisplayInformation^ display_info = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
    Windows::UI::Xaml::Window^ window = Windows::UI::Xaml::Window::Current;
    Windows::Foundation::Rect bounds = window->Bounds;

    ff::window_size size{};
    size.dpi_scale = this->dpi_scale();
    size.pixel_size = (ff::point_double(bounds.Width, bounds.Height) * size.dpi_scale).cast<int>();
    size.native_rotation = ::get_rotation(display_info->NativeOrientation);
    size.current_rotation = ::get_rotation(display_info->CurrentOrientation);

    return size;
}

double ff::window::dpi_scale()
{
    return Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi;
}

bool ff::window::active()
{
    return Windows::UI::Xaml::Window::Current->CoreWindow->ActivationMode != Windows::UI::Core::CoreWindowActivationMode::Deactivated;
}

bool ff::window::visible()
{
    return Windows::UI::Xaml::Window::Current->Visible;
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
