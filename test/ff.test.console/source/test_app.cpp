#include "pch.h"
#include "source/resource.h"

template<typename StateT>
static void run_app()
{
    ff::init_app_params app_params{};
    ff::init_ui_params ui_params{};

    app_params.create_initial_state_func = []()
    {
        return std::make_shared<StateT>();
    };

    ff::init_app init_app(app_params, ui_params);
    if (init_app)
    {
        ff::handle_messages_until_quit();
    }
}

static INT_PTR CALLBACK wait_dialog_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            ::SetTimer(hwnd, 1, 3000, nullptr);
            return 1;

        case WM_DESTROY:
            ::KillTimer(hwnd, 1);
            break;

        case WM_TIMER:
            ::PostMessage(hwnd, WM_COMMAND, IDOK, 0);
            break;

        case WM_COMMAND:
            if (wp == IDOK || wp == IDCANCEL)
            {
                ::EndDialog(hwnd, 1);
                return 1;
            }
            break;
    }

    return 0;
}

static void show_wait_dialog()
{
    ::DialogBox(ff::get_hinstance(), MAKEINTRESOURCE(IDD_WAIT_DIALOG), ff::window::main()->handle(), ::wait_dialog_proc);
}

namespace
{
    class coroutine_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override
        {
            this->show_wait_dialog_async();
            return {};
        }

    private:
        void show_wait_dialog_async()
        {
            if (!this->showed_dialog)
            {
                this->showed_dialog = true;
                ff::thread_dispatch::get_main()->post([]()
                    {
                        ::show_wait_dialog();
                    });
            }
        }

        bool showed_dialog{};
    };
}

void run_test_app()
{
    ::run_app<ff::state>();
}

void run_coroutine_app()
{
    ::run_app<::coroutine_state>();
}
