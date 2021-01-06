#include "pch.h"
#include "thread_dispatch.h"
#include "win_handle.h"

void ff::win_handle::close(HANDLE& handle)
{
    if (handle && handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(handle);
        handle = nullptr;
    }
}

ff::win_handle ff::win_handle::duplicate(HANDLE handle)
{
    HANDLE new_handle = nullptr;
    if (handle && !::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(), &new_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        assert(false);
    }

    return win_handle(new_handle);
}

ff::win_handle::win_handle(HANDLE handle)
    : handle(handle != INVALID_HANDLE_VALUE ? handle : nullptr)
{}

ff::win_handle::win_handle(win_handle&& other) noexcept
    : handle(other.handle)
{
    other.handle = nullptr;
}

ff::win_handle::win_handle()
    : handle(nullptr)
{}

ff::win_handle::~win_handle()
{
    this->close();
}

ff::win_handle& ff::win_handle::operator=(win_handle&& other) noexcept
{
    this->close();
    std::swap(this->handle, other.handle);
    return *this;
}

bool ff::win_handle::operator!() const
{
    return !this->handle;
}

ff::win_handle::operator bool() const
{
    return this->handle != nullptr;
}

ff::win_handle::operator HANDLE() const
{
    return this->handle;
}

ff::win_handle ff::win_handle::duplicate() const
{
    return win_handle::duplicate(this->handle);
}

void ff::win_handle::close()
{
    win_handle::close(this->handle);
}

#if !UWP_APP

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
HINSTANCE ff::get_hinstance()
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    return instance ? instance : ::GetModuleHandle(nullptr);
}

#endif

ff::win_handle ff::create_event(bool initial_set, bool manual_reset)
{
    HANDLE handle = ::CreateEventEx(nullptr, nullptr,
        (initial_set ? CREATE_EVENT_INITIAL_SET : 0) | (manual_reset ? CREATE_EVENT_MANUAL_RESET : 0),
        EVENT_ALL_ACCESS);
    return ff::win_handle(handle);
}

bool ff::is_event_set(HANDLE handle)
{
    assert(handle);
    return ::WaitForSingleObjectEx(handle, 0, FALSE) == WAIT_OBJECT_0;
}

bool ff::wait_for_event_and_reset(HANDLE handle)
{
    return ff::wait_for_handle(handle) && ::ResetEvent(handle);
}

bool ff::wait_for_handle(HANDLE handle)
{
    size_t completed_index;
    return ff::wait_for_any_handle(&handle, 1, completed_index) && completed_index == 0;
}

bool ff::wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms)
{
    ff::thread_dispatch* dispatch = ff::thread_dispatch::get(ff::thread_dispatch_type::none);
    if (dispatch)
    {
        return dispatch->wait_for_any_handle(handles, count, completed_index, timeout_ms);
    }

    while (true)
    {
        DWORD result = ::WaitForMultipleObjectsEx(static_cast<DWORD>(count), handles, FALSE, static_cast<DWORD>(timeout_ms), TRUE);
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
                else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + count)
                {
                    return false;
                }
            }
            break;

        case WAIT_TIMEOUT:
        case WAIT_FAILED:
            return false;

        case WAIT_IO_COMPLETION:
            break;
        }
    }
}

bool ff::wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms)
{
    ff::thread_dispatch* dispatch = ff::thread_dispatch::get(ff::thread_dispatch_type::none);
    if (dispatch)
    {
        return dispatch->wait_for_all_handles(handles, count, timeout_ms);
    }

    while (true)
    {
        DWORD result = ::WaitForMultipleObjectsEx(static_cast<DWORD>(count), handles, TRUE, static_cast<DWORD>(timeout_ms), TRUE);
        switch (result)
        {
            default:
                if (static_cast<size_t>(result) < count)
                {
                    return true;
                }
                else if (result >= WAIT_ABANDONED_0 && result < WAIT_ABANDONED_0 + count)
                {
                    return false;
                }
                break;

            case WAIT_TIMEOUT:
            case WAIT_FAILED:
                return false;

            case WAIT_IO_COMPLETION:
                break;
        }
    }
}
