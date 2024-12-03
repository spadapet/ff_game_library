#include "pch.h"
#include "source/resource.h"

template<class StateT>
static void run_app()
{
    ff::init_app_params app_params{};
    app_params.create_initial_state_func = []()
        {
            return std::make_shared<StateT>();
        };

    ff::init_app init_app(app_params);
    assert_ret(init_app);
    ff::handle_messages_until_quit();
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
    class test_app_state : public ff::state
    {
    public:
        test_app_state()
        {
            this->input_connection = ff::input::combined_devices().event_sink().connect([this](const ff::input_device_event& event)
            {
                std::scoped_lock lock(this->input_mutex);
                bool ignore = (event.type == ff::input_device_event_type::mouse_move ||
                    event.type == ff::input_device_event_type::touch_move) &&
                    event.type == this->last_input_event.type;

                if (!ignore)
                {
                    std::string_view name = "<invalid>";

                    switch (event.type)
                    {
                        case ff::input_device_event_type::key_char: name = "key_char"; break;
                        case ff::input_device_event_type::key_press: name = "key_press"; break;
                        case ff::input_device_event_type::mouse_move: name = "mouse_move"; break;
                        case ff::input_device_event_type::mouse_press: name = "mouse_press"; break;
                        case ff::input_device_event_type::mouse_wheel_x: name = "mouse_wheel_x"; break;
                        case ff::input_device_event_type::mouse_wheel_y: name = "mouse_wheel_y"; break;
                        case ff::input_device_event_type::touch_move: name = "touch_move"; break;
                        case ff::input_device_event_type::touch_press: name = "touch_press"; break;
                        default: assert(false); break;
                    }

                    std::ostringstream str;
                    str << name << ": id=" << event.id << ", pos=(" << event.pos.x << "," << event.pos.y << "), count=" << event.count << std::endl;
                    std::cout << str.str();
                }

                this->last_input_event = event;
            });
        }

        virtual std::shared_ptr<ff::state> advance_time() override
        {
            this->clear_color.y = std::fmod(clear_color.y + 1.0f / 512.0f, 1.0f);
            return {};
        }

        virtual void render(ff::dxgi::command_context_base& context, ff::render_targets& targets) override
        {
            ff::dxgi::target_base& target = targets.target(context);
            ff::dxgi::draw_ptr draw = ff::dxgi::global_draw_device().begin_draw(context, target);
            if (draw)
            {
                draw->draw_filled_rectangle(target.size().logical_pixel_rect<float>(), this->clear_color);
            }
        }

    private:
        DirectX::XMFLOAT4 clear_color{ 0, 0.125, 0, 1 };

        std::mutex input_mutex;
        ff::signal_connection input_connection{};
        ff::input_device_event last_input_event{};
    };

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
            co_await ff::task::resume_on_main();
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
            co_await ff::task::resume_on_main();
            co_return ::show_wait_dialog();
        }

        std::unique_ptr<ff::co_task<bool>> task;
    };
}

void run_test_app()
{
    ::run_app<::test_app_state>();
}

void run_test_coroutine_app()
{
    ::run_app<::coroutine_state>();
}
