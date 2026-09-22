#pragma once

#include "../base/signal.h"
#include "../base/string.h"

typedef struct ff_window_message
{
    HWND hwnd;
    UINT msg;
    WPARAM wp;
    LPARAM lp;
    LRESULT result;
    bool handled;
} ff_window_message;

typedef struct ff_window
{
    HWND hwnd;
    ff_signal signal; // args = ff_window_message
} ff_window;

bool ff_window_main_init(ff_window* window, ff_string_view title);
bool ff_window_message_init(ff_window* window);

ff_window* ff_window_main(void);
void ff_window_main_show(void);
bool ff_window_main_is_full_screen(void);
void ff_window_main_set_full_screen(bool value);

int ff_window_handle_messages(void);
