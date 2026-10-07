#include "pch.h"
#include "base/arena.h"
#include "base/assert.h"
#include "windows/dispatch.h"
#include "windows/task.h"

typedef struct task_entry
{
    struct task_entry* next_free;
    ff_task_func func;
    void* cookie;
} task_entry;

static SRWLOCK s_mutex;
static TP_CALLBACK_ENVIRON s_pool_env;
static PTP_CLEANUP_GROUP s_pool_cleanup;
static ff_arena s_arena;
static task_entry* s_free_list;
static LONG s_outstanding;
static bool s_valid;
static bool s_draining;

static void complete_task(void)
{
    InterlockedDecrement(&s_outstanding);
    WakeByAddressAll(&s_outstanding);
}

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
    AcquireSRWLockExclusive(&s_mutex);
    entry->next_free = s_free_list;
    s_free_list = entry;
    ReleaseSRWLockExclusive(&s_mutex);
}

static void CALLBACK task_callback(PTP_CALLBACK_INSTANCE instance, void* context)
{
    task_entry* entry = (task_entry*)context;
    const ff_task_func func = entry->func;
    void* cookie = entry->cookie;

    free_entry(entry);
    func(cookie);

    // Decremented last so a task that queues more work still counts as outstanding.
    complete_task();
}

static void drain(ff_dispatch* dispatch)
{
    if (!dispatch)
    {
        CloseThreadpoolCleanupGroupMembers(s_pool_cleanup, FALSE, NULL);
    }

    while (true)
    {
        LONG outstanding;

        AcquireSRWLockExclusive(&s_mutex);
        outstanding = InterlockedCompareExchange(&s_outstanding, 0, 0);
        if (!outstanding)
        {
            s_draining = false;
            ReleaseSRWLockExclusive(&s_mutex);
            break;
        }
        ReleaseSRWLockExclusive(&s_mutex);

        if (dispatch)
        {
            ff_dispatch_flush(dispatch);
            LONG waiting = outstanding;
            WaitOnAddress(&s_outstanding, &waiting, sizeof(waiting), (DWORD)1);
        }
        else
        {
            WaitOnAddress(&s_outstanding, &outstanding, sizeof(outstanding), INFINITE);
        }
    }
}

void ff_task_init(void)
{
    FF_ASSERT_RET(!s_valid);

    InitializeSRWLock(&s_mutex);
    ff_arena_init_heap_global(&s_arena, 0);
    s_free_list = NULL;
    InterlockedExchange(&s_outstanding, 0);
    s_draining = false;

    InitializeThreadpoolEnvironment(&s_pool_env);
    s_pool_cleanup = CreateThreadpoolCleanupGroup();
    SetThreadpoolCallbackCleanupGroup(&s_pool_env, s_pool_cleanup, NULL);
    s_valid = true;
}

static bool begin_destroy(void)
{
    bool valid;

    AcquireSRWLockExclusive(&s_mutex);
    valid = s_valid;
    s_valid = false;
    s_draining = valid;
    ReleaseSRWLockExclusive(&s_mutex);

    return valid;
}

static void finish_destroy(void)
{
    CloseThreadpoolCleanupGroup(s_pool_cleanup);
    s_pool_cleanup = NULL;
    DestroyThreadpoolEnvironment(&s_pool_env);

    s_free_list = NULL;
    ff_arena_destroy(&s_arena);
}

void ff_task_destroy(ff_dispatch* main_dispatch)
{
    if (!begin_destroy())
    {
        return;
    }

    drain(main_dispatch);
    if (main_dispatch)
    {
        CloseThreadpoolCleanupGroupMembers(s_pool_cleanup, FALSE, NULL);
    }

    finish_destroy();
}

void ff_task_add(ff_task_func func, void* cookie)
{
    FF_ASSERT_RET(func);

    bool submitted = false;
    bool tracked = false;

    AcquireSRWLockExclusive(&s_mutex);

    if (s_valid)
    {
        task_entry* entry = alloc_entry(func, cookie);
        InterlockedIncrement(&s_outstanding);
        tracked = true;
        submitted = TrySubmitThreadpoolCallback(&task_callback, entry, &s_pool_env) != FALSE;

        if (!submitted)
        {
            entry->next_free = s_free_list;
            s_free_list = entry;
        }
    }
    else if (s_draining)
    {
        InterlockedIncrement(&s_outstanding);
        tracked = true;
    }

    ReleaseSRWLockExclusive(&s_mutex);

    if (!submitted)
    {
        func(cookie);
        if (tracked)
        {
            complete_task();
        }
    }
}

void ff_task_flush(void)
{
    FF_CHECK_RET(s_valid);
    drain(NULL);
}
