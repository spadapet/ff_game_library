#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "windows/thread.h"

#define FF_WM_FLUSH (WM_USER + 0)

static void run_entries(ff_thread_dispatch* dispatch)
{
    while (true)
    {
        internal_ff_thread_dispatch_entry entries[16];
        size_t count = 0;

        EnterCriticalSection(&dispatch->mutex);
        {
            count = ff_array_count(dispatch->entries_a);

            if (count > _countof(entries))
            {
                count = _countof(entries);
            }

            if (count)
            {
                memcpy(entries, dispatch->entries_a, sizeof(internal_ff_thread_dispatch_entry) * count);
                memmove(dispatch->entries_a, dispatch->entries_a + count, sizeof(internal_ff_thread_dispatch_entry) * (ff_array_count(dispatch->entries_a) - count));
                ff_array_resize(dispatch->entries_a, ff_array_count(dispatch->entries_a) - count);
            }
            else
            {
                dispatch->posted = false;
                SetEvent(dispatch->flushed_event);
            }
        }
        LeaveCriticalSection(&dispatch->mutex);

        FF_CHECK_RET(count);

        for (size_t i = 0; i < count; i++)
        {
            entries[i].func(entries[i].cookie);
        }
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
    dispatch->destroyed = false;
    dispatch->flushed_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    InitializeCriticalSection(&dispatch->mutex);
    ff_arena_init_heap_local(&dispatch->arena, 0);
    dispatch->entries_a = ff_array_init(internal_ff_thread_dispatch_entry, &dispatch->arena);
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

    EnterCriticalSection(&dispatch->mutex);
    dispatch->destroyed = true;
    LeaveCriticalSection(&dispatch->mutex);

    run_entries(dispatch);

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
    ff_arena_destroy(&dispatch->arena);
    DeleteCriticalSection(&dispatch->mutex);
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
