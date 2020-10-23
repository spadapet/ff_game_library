#pragma once

namespace ff
{
    /// <summary>
    /// Replacement class for std::recursive_mutex
    /// </summary>
    /// <remarks>
    /// This is a drop-in replacement for the std::recursive_mutex class. It's here because the one in
    /// the standard library is too complicated. It's not clear when things are initialized or
    /// what type of system object is even used for synchronization. Also, the methods here are
    /// const as they should be since they are all thread safe.
    /// 
    /// The implemention is so simple, with a CRITICAL_SECTION object.
    /// If that sounds good, use this class, otherwise stick with std::recursive_mutex.
    /// </remarks>
    class recursive_mutex
    {
    public:
        recursive_mutex();
        ~recursive_mutex();

        void lock() const;
        bool try_lock() const;
        void unlock() const;

    private:
        recursive_mutex(const recursive_mutex& rhs) = delete;
        recursive_mutex(recursive_mutex&& rhs) = delete;
        const recursive_mutex& operator=(const recursive_mutex& rhs) = delete;

        mutable ::CRITICAL_SECTION cs;
    };

    /// <summary>
    /// Scoped lock for a ff::recursive_mutex
    /// </summary>
    /// <remarks>
    /// This works the same as std::lock_guard<T>, but there's no need for the template parameter.
    /// The nice thing here is the unlock function, which lets you release the lock early.
    /// </remarks>
    class lock_guard
    {
    public:
        lock_guard(const ff::recursive_mutex& mutex);
        lock_guard(lock_guard&& rhs);
        ~lock_guard();

        void unlock();

    private:
        lock_guard(const lock_guard& rhs) = delete;
        const lock_guard& operator=(const lock_guard& rhs) = delete;

        const ff::recursive_mutex* mutex;
    };
}
