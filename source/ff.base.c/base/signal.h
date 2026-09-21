#pragma once

typedef void (*ff_signal_func)(void* args, void* cookie);

typedef struct ff_signal_connection
{
    struct ff_signal_connection* prev;
    struct ff_signal_connection* next;
    ff_signal_func func;
    void* cookie;
} ff_signal_connection;

typedef struct ff_signal
{
    ff_signal_connection head;
} ff_signal;

void ff_signal_init(ff_signal* signal);
void ff_signal_destroy(ff_signal* signal);
void ff_signal_connect(ff_signal* signal, ff_signal_connection* connection, ff_signal_func func, void* cookie);
void ff_signal_notify(ff_signal* signal, void* args);

void ff_signal_connection_init(ff_signal_connection* connection);
void ff_signal_connection_init_and_connect(ff_signal_connection* connection, ff_signal* signal, ff_signal_func func, void* cookie);
void ff_signal_connection_destroy(ff_signal_connection* connection);
