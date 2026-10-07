#pragma once

typedef void (*ff_task_func)(void* cookie);
typedef struct ff_dispatch ff_dispatch;

void ff_task_init(void);
// Pass NULL when shutdown does not need to service the main-thread dispatcher.
void ff_task_destroy(ff_dispatch* main_dispatch);

void ff_task_add(ff_task_func func, void* cookie);
void ff_task_flush(void);
