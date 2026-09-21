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

HWND ff_window_main_init(ff_string_view title);
HWND ff_window_main(void);
void ff_window_main_show(void);
bool ff_window_main_is_full_screen(void);
void ff_window_main_set_full_screen(bool value);
ff_signal* ff_window_main_signal(void); // Notified with ff_window_message for every HWND message for the main window.

HWND ff_window_create_message(void);
int ff_window_handle_messages(void);
