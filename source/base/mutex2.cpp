#include "pch.h"
#include "mutex2.h"

ff::recursive_mutex::recursive_mutex()
{
    ::InitializeCriticalSectionEx(&this->cs, 3000, 0);
}

ff::recursive_mutex::~recursive_mutex()
{
    ::DeleteCriticalSection(&this->cs);
}

void ff::recursive_mutex::lock() const
{
    ::EnterCriticalSection(&this->cs);
}

bool ff::recursive_mutex::try_lock() const
{
    return ::TryEnterCriticalSection(&this->cs) != FALSE;
}

void ff::recursive_mutex::unlock() const
{
    ::LeaveCriticalSection(&this->cs);
}

ff::lock_guard::lock_guard(const ff::recursive_mutex& mutex)
    : mutex(&mutex)
{
    this->mutex->lock();
}

ff::lock_guard::lock_guard(lock_guard&& rhs)
    : mutex(rhs.mutex)
{
    rhs.mutex = nullptr;
}

ff::lock_guard::~lock_guard()
{
    this->unlock();
}

void ff::lock_guard::unlock()
{
    if (this->mutex != nullptr)
    {
        this->mutex->unlock();
        this->mutex = nullptr;
    }
}
