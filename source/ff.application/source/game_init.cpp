#include "pch.h"
#include "app.h"
#include "game_state_base.h"
#include "game_init.h"
#include "init.h"

static const ff::game::init_params* init_params{};
static std::weak_ptr<ff::game::app_state_base> app_state;

static std::shared_ptr<ff::state> create_app_state()
{
    auto app_state = ::init_params->create_initial_state();
    ::app_state = app_state;

    if (app_state)
    {
        app_state->internal_init();
    }

    return app_state;
}

static const ff::dxgi::palette_base* get_noesis_palette()
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->palette() : nullptr;
}

static double get_time_scale()
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->time_scale() : 1;
}

static ff::state::advance_t get_advance_type()
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->advance_type() : ff::state::advance_t::running;
}

static bool get_clear_color(DirectX::XMFLOAT4& color)
{
    auto app_state = ::app_state.lock();
    return app_state ? app_state->clear_color(color) : false;
}

static ff::init_app_params get_app_params()
{
    ff::init_app_params params{};
    params.create_initial_state_func = ::create_app_state;
    params.get_time_scale_func = ::get_time_scale;
    params.get_advance_type_func = ::get_advance_type;
    params.get_clear_color_func = ::get_clear_color;

    return params;
}

static ff::init_ui_params get_ui_params()
{
    ff::init_ui_params params{};
    params.application_resources_name = ::init_params->noesis_application_resources_name;
    params.noesis_license_name = ::init_params->noesis_license_name;
    params.noesis_license_key = ::init_params->noesis_license_key;
    params.palette_func = ::get_noesis_palette;
    params.register_components_func = ::init_params->register_global_resources;

    return params;
}

#if UWP_APP

namespace game
{
    class app : public winrt::implements<app, winrt::Windows::ApplicationModel::Core::IFrameworkViewSource, winrt::Windows::ApplicationModel::Core::IFrameworkView>
    {
    public:
        winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView()
        {
            return *this;
        }

        void Initialize(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& view)
        {
            view.Activated({ this, &app::activated });
        }

        void Load(const winrt::hstring& entry_point)
        {}

        void Run()
        {
            ff::handle_messages_until_quit();
        }

        void SetWindow(const winrt::Windows::UI::Core::CoreWindow& window)
        {}

        void Uninitialize()
        {}

    private:
        void activated(const winrt::Windows::ApplicationModel::Core::CoreApplicationView& view, const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& args)
        {
            if (!this->init_app)
            {
                auto app_view = winrt::Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
                app_view.SetPreferredMinSize(winrt::Windows::Foundation::Size(480, 270));

                this->init_app = std::make_unique<ff::init_app>(::get_app_params(), ::get_ui_params());
                assert(*this->init_app);
            }

            winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread().Activate();
        }

        std::unique_ptr<ff::init_app> init_app;
    };
}

int ff::game::run(const ff::game::init_params& params)
{
    ::init_params = &params;
    winrt::init_apartment();
    winrt::Windows::ApplicationModel::Core::CoreApplication::Run(winrt::make<::game::app>());
    return 0;
}

#else

static void set_window_client_size(HWND hwnd, const ff::point_int& new_size)
{
    RECT rect{ 0, 0, new_size.x, new_size.y }, rect2;
    const DWORD style = static_cast<DWORD>(GetWindowLong(hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLong(hwnd, GWL_EXSTYLE));

    HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);

    if (monitor &&
        ::AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, ::GetDpiForWindow(hwnd)) &&
        ::GetMonitorInfo(monitor, &mi) &&
        ::GetWindowRect(hwnd, &rect2))
    {
        ff::rect_int monitor_rect(mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom);
        ff::rect_int window_rect(rect2.left, rect2.top, rect2.left + rect.right - rect.left, rect2.top + rect.bottom - rect.top);

        window_rect = window_rect.move_inside(monitor_rect).crop(monitor_rect);

        ::SetWindowPos(hwnd, nullptr, window_rect.left, window_rect.top, window_rect.width(), window_rect.height(),
            SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
}

static void handle_window_message(ff::window_message& msg)
{
    switch (msg.msg)
    {
        case WM_GETMINMAXINFO:
            {
                RECT rect{ 0, 0, 240, 135 };
                const DWORD style = static_cast<DWORD>(::GetWindowLong(msg.hwnd, GWL_STYLE));
                const DWORD exStyle = static_cast<DWORD>(::GetWindowLong(msg.hwnd, GWL_EXSTYLE));
                if (::AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, ::GetDpiForWindow(msg.hwnd)))
                {
                    MINMAXINFO& mm = *reinterpret_cast<MINMAXINFO*>(msg.lp);
                    mm.ptMinTrackSize.x = rect.right - rect.left;
                    mm.ptMinTrackSize.y = rect.bottom - rect.top;
                }
            }
            break;

        case WM_SYSKEYDOWN:
            if (ff::constants::profile_build &&
                (msg.wp == VK_DELETE || msg.wp == VK_PRIOR || msg.wp == VK_NEXT) &&
                ::GetKeyState(VK_SHIFT) < 0 &&
                !ff::app_render_target().full_screen())
            {
                // Shift-Alt-Del: Force 1080p client area
                // Shift-Alt-PgUp|PgDown: Force multiples of 1080p client area

                RECT rect;
                if (::GetClientRect(msg.hwnd, &rect))
                {
                    const std::array<ff::point_int, 5> sizes
                    {
                        ff::point_int(240, 135),
                        ff::point_int(480, 270),
                        ff::point_int(960, 540),
                        ff::point_int(1920, 1080),
                        ff::point_int(3840, 2160),
                    };

                    const ff::point_int old_size(rect.right, rect.bottom);
                    ff::point_int new_size = (msg.wp == VK_DELETE) ? sizes[2] : old_size;

                    if (msg.wp == VK_PRIOR)
                    {
                        for (auto i = sizes.crbegin(); i != sizes.crend(); i++)
                        {
                            if (i->x < new_size.x)
                            {
                                new_size = *i;
                                break;
                            }
                        }
                    }
                    else if (msg.wp == VK_NEXT)
                    {
                        for (auto i = sizes.cbegin(); i != sizes.cend(); i++)
                        {
                            if (i->x > new_size.x)
                            {
                                new_size = *i;
                                break;
                            }
                        }
                    }

                    if (new_size != old_size)
                    {
                        ::set_window_client_size(msg.hwnd, new_size);
                    }
                }
            }
            break;
    }
}

int ff::game::run(const ff::game::init_params& params)
{
    ::init_params = &params;
    ff::init_app init_app(::get_app_params(), ::get_ui_params());
    ff::signal_connection message_connection = ff::window::main()->message_sink().connect(::handle_window_message);
    return ff::handle_messages_until_quit();
}

#endif
