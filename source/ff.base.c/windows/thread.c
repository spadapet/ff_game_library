#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "windows/thread.h"

#define FF_WM_FLUSH (WM_USER + 0)

typedef struct internal_ff_thread_dispatch_entry
{
    ff_thread_dispatch_func func;
    void* cookie;
} internal_ff_thread_dispatch_entry;

static void destroy_now(ff_thread_dispatch* dispatch)
{
    ff_signal_connection_destroy(&dispatch->connection);

    if (dispatch->window.hwnd)
    {
        DestroyWindow(dispatch->window.hwnd);
    }

    if (dispatch->flushed_event)
    {
        CloseHandle(dispatch->flushed_event);
        dispatch->flushed_event = NULL;
    }

    dispatch->entries_a = NULL;
    dispatch->running_entries_a = NULL;
    ff_arena_destroy(&dispatch->arena);
    DeleteCriticalSection(&dispatch->mutex);
}

static void run_entries(ff_thread_dispatch* dispatch)
{
    bool already_running;

    EnterCriticalSection(&dispatch->mutex);
    {
        already_running = dispatch->running;
        dispatch->running = true;
    }
    LeaveCriticalSection(&dispatch->mutex);

    // A callback that flushes re-entrantly returns here instead of draining. The loop below is
    // still running and picks up whatever the callback posted, so the work happens either way,
    // but a long post-then-flush chain iterates instead of recursing onto the stack.
    FF_CHECK_RET(!already_running);

    bool destroyed = false;

    while (true)
    {
        internal_ff_thread_dispatch_entry* entries_a = NULL;

        EnterCriticalSection(&dispatch->mutex);
        {
            if (ff_array_count(dispatch->entries_a))
            {
                // Swapping the batch out of entries_a keeps the drain O(n) and lets other threads
                // keep posting into the now-empty array while these callbacks run unlocked.
                entries_a = dispatch->entries_a;
                dispatch->entries_a = dispatch->running_entries_a;
                dispatch->running_entries_a = NULL;
            }
            else
            {
                dispatch->posted = false;
                dispatch->running = false;
                destroyed = dispatch->destroyed;
                SetEvent(dispatch->flushed_event);
            }
        }
        LeaveCriticalSection(&dispatch->mutex);

        if (!entries_a)
        {
            break;
        }

        const size_t count = ff_array_count(entries_a);

        for (size_t i = 0; i < count; i++)
        {
            entries_a[i].func(entries_a[i].cookie);
        }

        ff_array_resize(entries_a, 0);

        EnterCriticalSection(&dispatch->mutex);
        {
            dispatch->running_entries_a = entries_a;
        }
        LeaveCriticalSection(&dispatch->mutex);
    }

    // A callback called destroy while this loop owned the entry array, so it deferred to here.
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
        run_entries((ff_thread_dispatch*)cookie);
    }
}

bool ff_thread_dispatch_init(ff_thread_dispatch* dispatch)
{
    FF_ASSERT_RET_VAL(dispatch, false);

    dispatch->thread_id = GetCurrentThreadId();
    dispatch->posted = false;
    dispatch->running = false;
    dispatch->destroyed = false;
    dispatch->flushed_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    InitializeCriticalSection(&dispatch->mutex);
    ff_arena_init_heap_local(&dispatch->arena, 0);
    dispatch->entries_a = ff_array_init(internal_ff_thread_dispatch_entry, &dispatch->arena);
    dispatch->running_entries_a = ff_array_init(internal_ff_thread_dispatch_entry, &dispatch->arena);
    ff_signal_connection_init(&dispatch->connection);

    if (!dispatch->flushed_event || !ff_window_message_init(&dispatch->window))
    {
        ff_thread_dispatch_destroy(dispatch);
        FF_DEBUG_FAIL_RET_VAL(false);
    }

    ff_signal_connect(&dispatch->window.signal, &dispatch->connection, handle_message, dispatch);

    return true;
}

void ff_thread_dispatch_destroy(ff_thread_dispatch* dispatch)
{
    FF_CHECK_RET(dispatch && !dispatch->destroyed);
    FF_ASSERT_RET(ff_thread_dispatch_current_thread(dispatch));

    bool running;

    EnterCriticalSection(&dispatch->mutex);
    {
        dispatch->destroyed = true;
        running = dispatch->running;
    }
    LeaveCriticalSection(&dispatch->mutex);

    // A callback can destroy its own dispatcher. Tearing down here would free the arena out from
    // under the entry array that the drain loop is still walking, so let that loop finish first.
    FF_CHECK_RET(!running);

    run_entries(dispatch);

    destroy_now(dispatch);
}

void ff_thread_dispatch_post(ff_thread_dispatch* dispatch, ff_thread_dispatch_func func, void* cookie)
{
    FF_ASSERT_RET(dispatch && func);

    // After destroy the critical section is gone, so "destroyed" must be checked before taking it.
    if (dispatch->destroyed)
    {
        func(cookie);
        return;
    }

    bool run_now = false;

    EnterCriticalSection(&dispatch->mutex);
    {
        if (dispatch->destroyed)
        {
            run_now = true;
        }
        else
        {
            const internal_ff_thread_dispatch_entry entry = { .func = func, .cookie = cookie };
            ff_array_push(dispatch->entries_a, entry);
            ResetEvent(dispatch->flushed_event);

            if (!dispatch->posted)
            {
                dispatch->posted = true;
                PostMessage(dispatch->window.hwnd, FF_WM_FLUSH, 0, 0);
            }
        }
    }
    LeaveCriticalSection(&dispatch->mutex);

    if (run_now)
    {
        func(cookie);
    }
}

void ff_thread_dispatch_send(ff_thread_dispatch* dispatch, ff_thread_dispatch_func func, void* cookie)
{
    FF_ASSERT_RET(dispatch && func);

    if (ff_thread_dispatch_current_thread(dispatch))
    {
        func(cookie);
        return;
    }

    ff_thread_dispatch_post(dispatch, func, cookie);
    ff_thread_dispatch_flush(dispatch);
}

void ff_thread_dispatch_flush(ff_thread_dispatch* dispatch)
{
    FF_CHECK_RET(dispatch && !dispatch->destroyed);

    if (ff_thread_dispatch_current_thread(dispatch))
    {
        run_entries(dispatch);
    }
    else
    {
        WaitForSingleObject(dispatch->flushed_event, INFINITE);
    }
}

bool ff_thread_dispatch_current_thread(const ff_thread_dispatch* dispatch)
{
    return dispatch && dispatch->thread_id == GetCurrentThreadId();
}
