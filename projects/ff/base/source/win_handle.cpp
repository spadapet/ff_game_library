#include "pch.h"
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
