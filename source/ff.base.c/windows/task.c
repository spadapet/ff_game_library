#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "windows/task.h"

typedef struct task_entry
{
    struct task_entry* next_free;
    ff_task_func func;
    void* cookie;
} task_entry;

static CRITICAL_SECTION s_mutex;
static TP_CALLBACK_ENVIRON s_pool_env;
static PTP_CLEANUP_GROUP s_pool_cleanup;
static ff_arena s_arena;
static task_entry* s_free_list;
static LONG s_outstanding;
static bool s_valid;

static task_entry* alloc_entry(ff_task_func func, void* cookie)
{
    task_entry* entry = s_free_list;

    if (entry)
    {
        s_free_list = entry->next_free;
    }
    else
    {
        entry = ff_arena_alloc_type(&s_arena, task_entry, 1);
    }

    *entry = (task_entry)
    {
        .func = func,
        .cookie = cookie,
    };

    return entry;
}

static void free_entry(task_entry* entry)
{
    EnterCriticalSection(&s_mutex);
    entry->next_free = s_free_list;
    s_free_list = entry;
    LeaveCriticalSection(&s_mutex);
}

static void CALLBACK task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    task_entry* entry = (task_entry*)context;
    const ff_task_func func = entry->func;
    void* cookie = entry->cookie;

    free_entry(entry);
    func(cookie);

    // Decremented last so a task that queues more work still counts as outstanding.
    InterlockedDecrement(&s_outstanding);
}

static void drain(void)
{
    // CloseThreadpoolCleanupGroupMembers only waits on callbacks that were members when it
    // was called, so tasks queued by a running callback need another pass.
    do
    {
        CloseThreadpoolCleanupGroupMembers(s_pool_cleanup, FALSE, NULL);
    } while (InterlockedCompareExchange(&s_outstanding, 0, 0) != 0);
}

void ff_task_init(void)
{
    FF_ASSERT_RET(!s_valid);

    InitializeCriticalSection(&s_mutex);
    ff_arena_init_heap_global(&s_arena, 0);
    s_free_list = NULL;

    InitializeThreadpoolEnvironment(&s_pool_env);
    s_pool_cleanup = CreateThreadpoolCleanupGroup();
    SetThreadpoolCallbackCleanupGroup(&s_pool_env, s_pool_cleanup, NULL);
    s_valid = true;
}

void ff_task_destroy(void)
{
    FF_CHECK_RET(s_valid);

    EnterCriticalSection(&s_mutex);
    s_valid = false;
    LeaveCriticalSection(&s_mutex);

    drain();
    CloseThreadpoolCleanupGroup(s_pool_cleanup);
    s_pool_cleanup = NULL;
    DestroyThreadpoolEnvironment(&s_pool_env);

    s_free_list = NULL;
    ff_arena_destroy(&s_arena);
    DeleteCriticalSection(&s_mutex);
}

void ff_task_add(ff_task_func func, void* cookie)
{
    FF_ASSERT_RET(func);

    bool submitted = false;

    // s_valid is checked before taking the lock since the lock is gone after destroy.
    if (s_valid)
    {
        EnterCriticalSection(&s_mutex);

        if (s_valid)
        {
            // Submitted while holding the lock so destroy can't close the environment in between.
            task_entry* entry = alloc_entry(func, cookie);
            InterlockedIncrement(&s_outstanding);
            submitted = TrySubmitThreadpoolCallback(&task_callback, entry, &s_pool_env) != FALSE;

            if (!submitted)
            {
                InterlockedDecrement(&s_outstanding);
                entry->next_free = s_free_list;
                s_free_list = entry;
            }
        }

        LeaveCriticalSection(&s_mutex);
    }

    if (!submitted)
    {
        func(cookie);
    }
}

void ff_task_flush(void)
{
    FF_CHECK_RET(s_valid);
    drain();
}
