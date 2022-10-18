#include "pch.h"
#include "assert.h"
#include "thread_dispatch.h"
#include "win_msg.h"

static thread_local ff::thread_dispatch* task_thread_dispatch = nullptr;
static ff::thread_dispatch* main_thread_dispatch = nullptr;
static ff::thread_dispatch* game_thread_dispatch = nullptr;

ff::thread_dispatch::thread_dispatch(thread_dispatch_type type)
    : flushed_event(ff::win_handle::create_event(true))
    , pending_event(ff::win_handle::create_event())
    , thread_id(::GetCurrentThreadId())
    , destroyed(false)
#if UWP_APP
    , dispatcher(nullptr)
#else
    , message_window(ff::window::create_message_window())
    , message_window_connection(this->message_window.message_sink().connect(std::bind(&thread_dispatch::handle_message, this, std::placeholders::_1)))
#endif
{
#if UWP_APP
    winrt::Windows::UI::Core::CoreWindow window = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
    if (window)
    {
        this->dispatcher = window.Dispatcher();
        this->handler = winrt::Windows::UI::Core::DispatchedHandler([this]() { this->flush(true); });
    }
#endif

    switch (type)
    {
        case thread_dispatch_type::main:
            assert(!::main_thread_dispatch);
            ::main_thread_dispatch = this;
            break;

        case thread_dispatch_type::game:
            assert(!::game_thread_dispatch);
            ::game_thread_dispatch = this;
            break;
    }

    assert(!::task_thread_dispatch);
    ::task_thread_dispatch = this;
}

ff::thread_dispatch::~thread_dispatch()
{
    // Don't allow new dispatches
    {
        std::scoped_lock lock(this->mutex);
        this->destroyed = true;
    }

    this->flush(true);

    if (::main_thread_dispatch == this)
    {
        ::main_thread_dispatch = nullptr;
    }

    if (::game_thread_dispatch == this)
    {
        ::game_thread_dispatch = nullptr;
    }

    if (::task_thread_dispatch == this)
    {
        ::task_thread_dispatch = nullptr;
    }
}

ff::thread_dispatch* ff::thread_dispatch::get(thread_dispatch_type type)
{
    switch (type)
    {
        case thread_dispatch_type::game:
            if (::game_thread_dispatch)
            {
                return ::game_thread_dispatch;
            }
            [[fallthrough]];

        case thread_dispatch_type::main:
            if (::main_thread_dispatch)
            {
                return ::main_thread_dispatch;
            }
            [[fallthrough]];

        default:
            return ::task_thread_dispatch;
    }
}

ff::thread_dispatch* ff::thread_dispatch::get_main()
{
    return ff::thread_dispatch::get(ff::thread_dispatch_type::main);
}

ff::thread_dispatch* ff::thread_dispatch::get_game()
{
    return ff::thread_dispatch::get(ff::thread_dispatch_type::game);
}

ff::thread_dispatch_type ff::thread_dispatch::get_type()
{
    if (ff::thread_dispatch::get_main()->current_thread())
    {
        return ff::thread_dispatch_type::main;
    }

    if (ff::thread_dispatch::get_game()->current_thread())
    {
        return ff::thread_dispatch_type::game;
    }

    return ff::thread_dispatch_type::task;
}

void ff::thread_dispatch::post(std::function<void()>&& func, bool run_if_current_thread)
{
    if (!this || (run_if_current_thread && this->current_thread()))
    {
        func();
        return;
    }

    std::scoped_lock lock(this->mutex);

    if (this->destroyed)
    {
        func();
        return;
    }

    bool was_empty = this->funcs.empty();
    this->funcs.push_front(std::move(func));

    if (was_empty)
    {
        ::ResetEvent(this->flushed_event);

        if (!ff::is_event_set(this->pending_event))
        {
            ::SetEvent(this->pending_event);
            this->post_flush();
        }
    }
}

void ff::thread_dispatch::send(std::function<void()>&& func)
{
    if (this->current_thread())
    {
        func();
    }
    else
    {
        ff::win_handle event = ff::win_handle::create_event();
        std::function<void()> func_2 = std::move(func);

        this->post([&event, &func_2]()
        {
            func_2();
            ::SetEvent(event);
        });

        ff::wait_for_handle(event);
    }
}

void ff::thread_dispatch::flush()
{
    this->flush(false);
}

bool ff::thread_dispatch::current_thread() const
{
    return this && this->thread_id == ::GetCurrentThreadId();
}

bool ff::thread_dispatch::wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms)
{
    assert(count <= ff::thread_dispatch::maximum_wait_objects);
    std::vector<HANDLE> handles_vector(std::initializer_list(handles, handles + count));
    handles_vector.push_back(this->pending_event);
    ULONGLONG cur_tick = ::GetTickCount64();
    ULONGLONG end_tick = cur_tick + timeout_ms;

    while (end_tick >= cur_tick)
    {
        DWORD wait_ms = (timeout_ms != INFINITE) ? static_cast<DWORD>(end_tick - cur_tick) : INFINITE;
        DWORD wait_size = static_cast<DWORD>(handles_vector.size());
#if UWP_APP
        DWORD result = ::WaitForMultipleObjectsEx(wait_size, handles_vector.data(), FALSE, wait_ms, TRUE);
#else
        DWORD result = ::MsgWaitForMultipleObjectsEx(wait_size, handles_vector.data(), wait_ms, QS_ALLINPUT, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
#endif
        switch (result)
        {
            default:
                {
                    size_t size_result = static_cast<size_t>(result);
                    if (size_result < count)
                    {
                        completed_index = size_result;
                        return true;
                    }
                    else if (size_result == count)
                    {
                        this->flush(false);

                        if (!count)
                        {
                            return true;
                        }
                    }
                    else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + count + 1)
                    {
                        assert(false);
                        return false;
                    }
                }
                break;

            case WAIT_TIMEOUT:
                return false;

            case WAIT_FAILED:
                assert(false);
                return false;

            case WAIT_IO_COMPLETION:
                break;
        }

#if !UWP_APP
        ff::handle_messages();
#endif
        if (cur_tick == end_tick)
        {
            break;
        }

        cur_tick = ::GetTickCount64();
    }

    // timeout
    return false;
}

bool ff::thread_dispatch::wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms)
{
    std::vector<HANDLE> handle_vector(std::initializer_list(handles, handles + count));

    while (!handle_vector.empty())
    {
        size_t completed;
        if (!this->wait_for_any_handle(handle_vector.data(), std::min(handle_vector.size(), ff::thread_dispatch::maximum_wait_objects), completed, timeout_ms))
        {
            return false;
        }

        assert(completed < handle_vector.size());
        handle_vector.erase(handle_vector.cbegin() + completed);
    }

    return true;
}

void ff::thread_dispatch::flush(bool force)
{
#if !UWP_APP
    if (ff::thread_dispatch::get_main() != this)
    {
        // background threads might only call flush to pump messages
        ff::handle_messages();
    }
#endif

    if (force || this->current_thread())
    {
        std::forward_list<std::function<void()>> funcs;
        {
            std::scoped_lock lock(this->mutex);
            funcs = std::move(this->funcs);
        }

        while (!funcs.empty())
        {
            funcs.reverse();
            for (auto& func : funcs)
            {
                func();
            }

            std::scoped_lock lock(this->mutex);
            funcs = std::move(this->funcs);

            if (funcs.empty())
            {
                ::SetEvent(this->flushed_event);
                ::ResetEvent(this->pending_event);
            }
        }
    }
    else
    {
        ff::wait_for_handle(this->flushed_event);
    }
}

void ff::thread_dispatch::post_flush()
{
#if UWP_APP
    if (this->dispatcher)
    {
        this->dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, this->handler);
    }
#else
    ::PostMessage(this->message_window, WM_USER, 0, 0);
#endif
}

#if !UWP_APP

void ff::thread_dispatch::handle_message(ff::window_message& msg)
{
    if (msg.msg == WM_USER || msg.msg == WM_DESTROY)
    {
        this->flush(true);
    }
}

#endif
