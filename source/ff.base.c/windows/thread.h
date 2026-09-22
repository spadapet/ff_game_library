#pragma once

#include "../base/arena.h"
#include "../base/signal.h"
#include "window.h"

typedef void (*ff_thread_dispatch_func)(void* cookie);
typedef struct internal_ff_thread_dispatch_entry internal_ff_thread_dispatch_entry;

typedef struct ff_thread_dispatch
{
    ff_window window;
    ff_signal_connection connection;
    CRITICAL_SECTION mutex;
    HANDLE flushed_event;
    ff_arena arena;
    internal_ff_thread_dispatch_entry* entries_a;
    internal_ff_thread_dispatch_entry* running_entries_a; // swapped with entries_a to drain without holding the lock
    DWORD thread_id;
    bool posted;
    bool running;
    bool destroyed;
} ff_thread_dispatch;

bool ff_thread_dispatch_init(ff_thread_dispatch* dispatch);
void ff_thread_dispatch_destroy(ff_thread_dispatch* dispatch);

void ff_thread_dispatch_post(ff_thread_dispatch* dispatch, ff_thread_dispatch_func func, void* cookie);
void ff_thread_dispatch_send(ff_thread_dispatch* dispatch, ff_thread_dispatch_func func, void* cookie);
void ff_thread_dispatch_flush(ff_thread_dispatch* dispatch);
bool ff_thread_dispatch_current_thread(const ff_thread_dispatch* dispatch);
