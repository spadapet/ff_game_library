#include "pch.h"
#include "source/resource.h"

template<class StateT>
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

static int show_wait_dialog()
{
    assert(ff::thread_dispatch::get_main()->current_thread());
    INT_PTR result = ::DialogBox(ff::get_hinstance(), MAKEINTRESOURCE(IDD_WAIT_DIALOG), ff::window::main()->handle(), ::wait_dialog_proc);
    return static_cast<int>(result);
}

namespace
{
    class coroutine_state : public ff::state
    {
    public:
        virtual std::shared_ptr<ff::state> advance_time() override
        {
            if (!this->task)
            {
                auto task = this->show_wait_dialog_async();
                this->task = std::make_unique<ff::co_task<bool>>(std::move(task));
            }
            else if (this->task->done())
            {
                this->task = {};
                ff::window::main()->close();
                return std::make_shared<ff::state>();
            }

            return {};
        }

    private:
        ff::co_task<bool> show_wait_dialog_async()
        {
            co_await ff::resume_on_main();
            Sleep(500);
            int result = co_await this->show_wait_dialog_async2();
            co_return result == IDOK;
        }

        ff::co_task<> await_task()
        {
            bool result = co_await *this->task;
            assert(result);
        }

        ff::co_task<int> show_wait_dialog_async2()
        {
            co_await ff::resume_on_main();
            co_return ::show_wait_dialog();
        }

        std::unique_ptr<ff::co_task<bool>> task;
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
