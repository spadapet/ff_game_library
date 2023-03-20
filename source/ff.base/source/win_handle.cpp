#include "pch.h"
#include "assert.h"
#include "co_task.h"
#include "stash.h"
#include "frame_allocator.h"
#include "thread_dispatch.h"
#include "win_handle.h"

// static ff::internal::stash<ff::internal::win_event_data> event_stash;

static constexpr bool is_valid_handle(HANDLE handle)
{
    return handle && handle != INVALID_HANDLE_VALUE;
}

void ff::internal::win_event_data::add_ref()
{
    this->refs.fetch_add(1);
}

void ff::internal::win_event_data::release_ref()
{
    if (this->refs.fetch_sub(1) == 1)
    {
        // ::event_stash.stash_obj(this);
    }
}

void ff::win_handle::close(HANDLE& handle)
{
    HANDLE local_handle = handle;
    handle = nullptr;

    if (::is_valid_handle(local_handle))
    {
        ::CloseHandle(local_handle);
    }
}

HANDLE ff::win_handle::duplicate(HANDLE handle)
{
    HANDLE new_handle = nullptr;

    if (::is_valid_handle(handle) && !::DuplicateHandle(::GetCurrentProcess(), handle, ::GetCurrentProcess(), &new_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        debug_fail_ret_val(nullptr);
    }

    return new_handle;
}

ff::win_handle::win_handle(HANDLE handle)
    : handle(::is_valid_handle(handle) ? handle : nullptr)
{}

ff::win_handle::win_handle(win_handle&& other) noexcept
    : handle(other.handle)
{
    other.handle = nullptr;
}

ff::win_handle::win_handle(const win_handle& other)
    : handle(nullptr)
{
    *this = other;
}

ff::win_handle::~win_handle()
{
    this->close();
}

ff::win_handle& ff::win_handle::operator=(win_handle&& other) noexcept
{
    if (this != &other)
    {
        this->close();
        std::swap(this->handle, other.handle);
    }

    return *this;
}

ff::win_handle& ff::win_handle::operator=(const win_handle& other)
{
    if (this != &other)
    {
        this->close();
        this->handle = ff::win_handle::duplicate(other.handle);
    }

    return *this;
}

ff::internal::co_handle_awaiter ff::win_handle::operator co_await()
{
    return ff::task::wait_handle(this->handle);
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

void ff::win_handle::close()
{
    win_handle::close(this->handle);
}

bool ff::win_handle::wait(size_t timeout_ms, bool allow_dispatch) const
{
    return this->handle && ff::wait_for_handle(this->handle, timeout_ms, allow_dispatch);
}

bool ff::win_handle::is_set() const
{
    return this->wait(0);
}

#if !UWP_APP

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
HINSTANCE ff::get_hinstance()
{
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    return instance ? instance : ::GetModuleHandle(nullptr);
}

#endif

ff::win_handle ff::win_handle::create_event(bool initial_set)
{
    return ff::win_handle(::CreateEventEx(nullptr, nullptr,
        (initial_set ? CREATE_EVENT_INITIAL_SET : 0) | CREATE_EVENT_MANUAL_RESET,
        EVENT_ALL_ACCESS));
}

const ff::win_handle& ff::win_handle::never_complete_event()
{
    static ff::win_handle handle = ff::win_handle::create_event();
    return handle;
}

const ff::win_handle& ff::win_handle::always_complete_event()
{
    static ff::win_handle handle = ff::win_handle::create_event(true);
    return handle;
}

bool ff::wait_for_event_and_reset(HANDLE handle, size_t timeout_ms, bool allow_dispatch)
{
    return ff::wait_for_handle(handle, timeout_ms, allow_dispatch) && ::ResetEvent(handle);
}

bool ff::wait_for_handle(HANDLE handle, size_t timeout_ms, bool allow_dispatch)
{
    size_t completed_index;
    return ff::wait_for_any_handle(&handle, 1, completed_index, timeout_ms, allow_dispatch) && completed_index == 0;
}

bool ff::wait_for_any_handle(const HANDLE* handles, size_t count, size_t& completed_index, size_t timeout_ms, bool allow_dispatch)
{
    if (timeout_ms && allow_dispatch)
    {
        ff::thread_dispatch* dispatch = ff::thread_dispatch::get();
        if (dispatch && dispatch->allow_dispatch_during_wait())
        {
            return dispatch->wait_for_any_handle(handles, count, completed_index, timeout_ms);
        }
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

bool ff::wait_for_all_handles(const HANDLE* handles, size_t count, size_t timeout_ms, bool allow_dispatch)
{
    if (timeout_ms && allow_dispatch)
    {
        ff::thread_dispatch* dispatch = ff::thread_dispatch::get();
        if (dispatch && dispatch->allow_dispatch_during_wait())
        {
            return dispatch->wait_for_all_handles(handles, count, timeout_ms);
        }
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
