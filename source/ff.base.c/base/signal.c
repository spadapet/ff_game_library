#include "pch.h"
#include "base/assert.h"
#include "base/signal.h"

static void link_to_self(ff_signal_connection* connection)
{
    connection->prev = connection;
    connection->next = connection;
}

static void unlink_node(ff_signal_connection* connection)
{
    connection->prev->next = connection->next;
    connection->next->prev = connection->prev;
    link_to_self(connection);
}

static void insert_node_before(ff_signal_connection* next, ff_signal_connection* connection)
{
    connection->prev = next->prev;
    connection->next = next;
    next->prev->next = connection;
    next->prev = connection;
}

// A zeroed connection has never been linked, so treat it as its own empty list.
static void ensure_linked(ff_signal_connection* connection)
{
    if (!connection->next)
    {
        link_to_self(connection);
    }
}

void ff_signal_init(ff_signal* signal)
{
    signal->head.func = NULL;
    signal->head.cookie = NULL;
    link_to_self(&signal->head);
}

void ff_signal_destroy(ff_signal* signal)
{
    ensure_linked(&signal->head);

    while (signal->head.next != &signal->head)
    {
        unlink_node(signal->head.next);
    }
}

void ff_signal_connection_init(ff_signal_connection* connection)
{
    FF_ASSERT_RET(connection);

    connection->func = NULL;
    connection->cookie = NULL;
    link_to_self(connection);
}

void ff_signal_connection_destroy(ff_signal_connection* connection)
{
    FF_ASSERT_RET(connection);
    ensure_linked(connection);
    unlink_node(connection);

    connection->func = NULL;
    connection->cookie = NULL;
}

void ff_signal_connect(ff_signal* signal, ff_signal_connection* connection, ff_signal_func func, void* cookie)
{
    FF_ASSERT_RET(signal && connection && func);
    ensure_linked(&signal->head);
    ensure_linked(connection);
    unlink_node(connection);

    connection->func = func;
    connection->cookie = cookie;
    insert_node_before(&signal->head, connection);
}

void ff_signal_notify(ff_signal* signal, void* args)
{
    ensure_linked(&signal->head);

    // The end sentinel bounds the walk to the handlers connected right now, so a handler that
    // reconnects itself or connects a new handler can't be revisited in this same notify.
    ff_signal_connection end;
    ff_signal_connection_init(&end);
    insert_node_before(&signal->head, &end);

    // The cursor is a real list member, so handlers may connect or disconnect anything,
    // including themselves or their neighbors, without dislodging the walk. Destroying the
    // whole signal unlinks the cursor along with everything else, which ends the walk.
    ff_signal_connection cursor;
    ff_signal_connection_init(&cursor);
    insert_node_before(signal->head.next, &cursor);

    while (cursor.next != &cursor && cursor.next != &end)
    {
        ff_signal_connection* connection = cursor.next;
        unlink_node(&cursor);
        insert_node_before(connection->next, &cursor);

        if (connection->func)
        {
            connection->func(args, connection->cookie);
        }
    }

    unlink_node(&cursor);
    unlink_node(&end);
}
