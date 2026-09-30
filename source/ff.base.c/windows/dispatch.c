#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "windows/dispatch.h"

#define FF_WM_FLUSH (WM_USER + 0)

typedef struct internal_ff_dispatch_entry
{
    ff_dispatch_func func;
    void* cookie;
    LONG* done; // only set by ff_dispatch_send, which waits on it
} internal_ff_dispatch_entry;

// Runs an entry and releases whoever is waiting on it. Every path that consumes an entry has to
// go through here: a waiter that never gets signaled blocks forever.
static void run_entry(const internal_ff_dispatch_entry* entry)
{
    if (entry->func)
    {
        entry->func(entry->cookie);
    }

    if (entry->done)
    {
        InterlockedExchange(entry->done, 1);
        WakeByAddressSingle(entry->done);
    }
}

static ff_dispatch* s_main_dispatch;
static ff_dispatch* s_game_dispatch;
static __declspec(thread) ff_dispatch* s_current_dispatch;

static void unregister_dispatch(ff_dispatch* dispatch)
{
    if (s_main_dispatch == dispatch)
    {
        s_main_dispatch = NULL;
    }

    if (s_game_dispatch == dispatch)
    {
        s_game_dispatch = NULL;
    }

    // Only clears the slot for the thread running destroy, which is the thread that set it.
    if (s_current_dispatch == dispatch)
    {
        s_current_dispatch = NULL;
    }
}

static void destroy_now(ff_dispatch* dispatch)
{
    if (dispatch->destroy_complete)
    {
        return;
    }

    dispatch->destroy_complete = true;
    unregister_dispatch(dispatch);
    ff_signal_connection_destroy(&dispatch->connection);

    if (dispatch->window.hwnd)
    {
        DestroyWindow(dispatch->window.hwnd);
        dispatch->window.hwnd = NULL;
    }

    dispatch->entries_a = NULL;
    dispatch->running_entries_a = NULL;
    ff_arena_destroy(&dispatch->arena);
}

static void run_entries(ff_dispatch* dispatch)
{
    bool already_running;

    AcquireSRWLockExclusive(&dispatch->mutex);
    {
        already_running = dispatch->running;
        dispatch->running = true;
    }
    ReleaseSRWLockExclusive(&dispatch->mutex);

    FF_CHECK_RET(!already_running);

    bool destroyed = false;
    while (true)
    {
        internal_ff_dispatch_entry* entries_a = NULL;

        AcquireSRWLockExclusive(&dispatch->mutex);
        {
            if (ff_array_count(dispatch->entries_a))
            {
                entries_a = dispatch->entries_a;
                dispatch->entries_a = dispatch->running_entries_a;
                dispatch->running_entries_a = NULL;
            }
            else
            {
                dispatch->posted = false;
                dispatch->running = false;
                destroyed = dispatch->destroyed;
            }
        }
        ReleaseSRWLockExclusive(&dispatch->mutex);

        if (!entries_a)
        {
            break;
        }

        const size_t count = ff_array_count(entries_a);

        for (size_t i = 0; i < count; i++)
        {
            run_entry(&entries_a[i]);
        }

        ff_array_resize(entries_a, 0);

        AcquireSRWLockExclusive(&dispatch->mutex);
        {
            dispatch->running_entries_a = entries_a;
        }
        ReleaseSRWLockExclusive(&dispatch->mutex);
    }

    if (destroyed)
    {
        destroy_now(dispatch);
    }
}

static void handle_message(void* args, void* cookie)
{
    const ff_window_message* message = (const ff_window_message*)args;

    if (message->msg == FF_WM_FLUSH)
    {
        run_entries((ff_dispatch*)cookie);
    }
}

bool ff_dispatch_init(ff_dispatch* dispatch, ff_dispatch_type type)
{
    FF_ASSERT_RET_VAL(dispatch, false);
    FF_ASSERT_RET_VAL(type != ff_dispatch_type_main || !s_main_dispatch, false);
    FF_ASSERT_RET_VAL(type != ff_dispatch_type_game || !s_game_dispatch, false);

    dispatch->window = (ff_window){ 0 };
    dispatch->thread_id = GetCurrentThreadId();
    dispatch->type = type;
    dispatch->posted = false;
    dispatch->running = false;
    dispatch->destroyed = false;
    dispatch->destroy_complete = false;
    InitializeSRWLock(&dispatch->mutex);
    ff_arena_init_heap_local(&dispatch->arena, 0);
    dispatch->entries_a = ff_array_init(internal_ff_dispatch_entry, &dispatch->arena);
    dispatch->running_entries_a = ff_array_init(internal_ff_dispatch_entry, &dispatch->arena);
    ff_signal_connection_init(&dispatch->connection);

    if (!ff_window_message_init(&dispatch->window))
    {
        dispatch->destroyed = true;
        destroy_now(dispatch);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    ff_signal_connect(&dispatch->window.signal, &dispatch->connection, handle_message, dispatch);

    switch (type)
    {
        case ff_dispatch_type_main:
            s_main_dispatch = dispatch;
            break;

        case ff_dispatch_type_game:
            s_game_dispatch = dispatch;
            break;

        default:
            break;
    }

    s_current_dispatch = dispatch;

    return true;
}

ff_dispatch* ff_dispatch_get_main(void)
{
    return s_main_dispatch;
}

ff_dispatch* ff_dispatch_get_game(void)
{
    return s_game_dispatch;
}

ff_dispatch* ff_dispatch_get_current(void)
{
    return s_current_dispatch;
}

void ff_dispatch_destroy(ff_dispatch* dispatch)
{
    FF_CHECK_RET(dispatch);
    FF_ASSERT_RET(ff_dispatch_is_current(dispatch));

    bool destroyed;
    bool running;
    AcquireSRWLockExclusive(&dispatch->mutex);
    {
        destroyed = dispatch->destroyed;
        running = dispatch->running;

        if (!destroyed)
        {
            dispatch->destroyed = true;
        }
    }
    ReleaseSRWLockExclusive(&dispatch->mutex);

    FF_CHECK_RET(!destroyed);
    unregister_dispatch(dispatch);

    if (!running)
    {
        run_entries(dispatch);
    }
}

static void post_entry(ff_dispatch* dispatch, const internal_ff_dispatch_entry* entry)
{
    bool run_now = false;
    bool send_flush = false;
    HWND hwnd = NULL;

    AcquireSRWLockExclusive(&dispatch->mutex);
    {
        if (dispatch->destroyed)
        {
            run_now = true;
        }
        else
        {
            ff_array_push(dispatch->entries_a, *entry);

            if (!dispatch->posted)
            {
                dispatch->posted = true;
                hwnd = dispatch->window.hwnd;
                send_flush = !PostMessage(hwnd, FF_WM_FLUSH, 0, 0);
            }
        }
    }
    ReleaseSRWLockExclusive(&dispatch->mutex);

    if (run_now)
    {
        run_entry(entry);
    }
    else if (send_flush)
    {
        if (hwnd)
        {
            SendMessage(hwnd, FF_WM_FLUSH, 0, 0);
        }

        if (!hwnd || (entry->done && !InterlockedCompareExchange(entry->done, 0, 0)))
        {
            run_entries(dispatch);
        }
    }
}

void ff_dispatch_post(ff_dispatch* dispatch, ff_dispatch_func func, void* cookie)
{
    FF_ASSERT_RET(dispatch && func);

    const internal_ff_dispatch_entry entry = { .func = func, .cookie = cookie };
    post_entry(dispatch, &entry);
}

void ff_dispatch_send(ff_dispatch* dispatch, ff_dispatch_func func, void* cookie)
{
    FF_ASSERT_RET(dispatch);

    if (ff_dispatch_is_current(dispatch))
    {
        if (func)
        {
            func(cookie);
        }

        return;
    }

    LONG done = 0;
    const internal_ff_dispatch_entry entry = { .func = func, .cookie = cookie, .done = &done };
    post_entry(dispatch, &entry);

    // Waits only for this entry, so a long batch queued behind it doesn't hold the caller up.
    LONG waiting = 0;
    while (!InterlockedCompareExchange(&done, 0, 0))
    {
        WaitOnAddress(&done, &waiting, sizeof(done), INFINITE);
    }
}

void ff_dispatch_flush(ff_dispatch* dispatch)
{
    FF_CHECK_RET(dispatch && !dispatch->destroyed);

    if (ff_dispatch_is_current(dispatch))
    {
        run_entries(dispatch);
    }
    else
    {
        // Waiting for the queue to empty would let other threads posting work starve this one, so
        // only wait for a barrier behind the work that was already queued.
        ff_dispatch_send(dispatch, NULL, NULL);
    }
}

bool ff_dispatch_is_current(const ff_dispatch* dispatch)
{
    return dispatch && dispatch->thread_id == GetCurrentThreadId();
}
