#pragma once

#include "../base/string.h"

typedef struct ff_signal ff_signal;

typedef struct ff_window_message
{
    HWND hwnd;
    UINT msg;
    WPARAM wp;
    LPARAM lp;
    LRESULT result;
    bool handled;
} ff_window_message;

HWND ff_window_create_main(ff_string_view title);
HWND ff_window_main(void);
void ff_window_show(void);

ff_signal* ff_window_message_signal(void);
int ff_window_message_loop(void);

bool ff_window_full_screen(void);
void ff_window_set_full_screen(bool value);
