#pragma once

#include "../base/arena.h"
#include "../base/signal.h"
#include "window.h"

typedef void (*ff_dispatch_func)(void* cookie);
typedef struct internal_ff_dispatch_entry internal_ff_dispatch_entry;

typedef enum ff_dispatch_type
{
    ff_dispatch_type_none,
    ff_dispatch_type_main,
    ff_dispatch_type_game,
} ff_dispatch_type;

typedef struct ff_dispatch
{
    ff_window window;
    ff_signal_connection connection;
    CRITICAL_SECTION mutex;
    HANDLE flushed_event;
    ff_arena arena;
    internal_ff_dispatch_entry* entries_a;
    internal_ff_dispatch_entry* running_entries_a;
    DWORD thread_id;
    ff_dispatch_type type;
    bool posted;
    bool running;
    bool destroyed;
} ff_dispatch;

bool ff_dispatch_init(ff_dispatch* dispatch, ff_dispatch_type type);
void ff_dispatch_destroy(ff_dispatch* dispatch);

ff_dispatch* ff_dispatch_get_main(void);
ff_dispatch* ff_dispatch_get_game(void);
ff_dispatch* ff_dispatch_get_current(void);

void ff_dispatch_post(ff_dispatch* dispatch, ff_dispatch_func func, void* cookie);
void ff_dispatch_send(ff_dispatch* dispatch, ff_dispatch_func func, void* cookie);
void ff_dispatch_flush(ff_dispatch* dispatch);
bool ff_dispatch_is_current(const ff_dispatch* dispatch);
