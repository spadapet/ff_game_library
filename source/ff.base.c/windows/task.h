#pragma once

typedef void (*ff_task_func)(void* cookie);

void ff_task_init(void);
void ff_task_destroy(void);

void ff_task_add(ff_task_func func, void* cookie);
void ff_task_flush(void);
