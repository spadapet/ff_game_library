#include "pch.h"

// Tests for ff_signal: an intrusive, allocation-free signal/slot list.
// Coverage:
//   * Basic connect/notify/disconnect and argument delivery.
//   * Independent lifetimes: either the signal or a connection may be destroyed first,
//     in any order, and the survivor stays valid.
//   * Re-entrancy: handlers may connect or disconnect anything (themselves, the next
//     handler, the whole list) during a notify without corrupting the walk.

namespace ff::test::base
{
    // Walking the list is only useful for verifying these tests, so the helpers live here
    // instead of in the public API.
    static size_t ff_signal_count(const ff_signal* signal)
    {
        size_t count = 0;

        for (const ff_signal_connection* i = signal->head.next; i && i != &signal->head; i = i->next)
        {
            count += (i->func != nullptr);
        }

        return count;
    }

    static bool ff_signal_connection_connected(const ff_signal_connection* connection)
    {
        return connection->next && connection->next != connection;
    }

    struct counter
    {
        int calls;
        int last_value;
    };

    static void count_handler(void* args, void* cookie)
    {
        counter* state = (counter*)cookie;
        state->calls++;
        state->last_value = args ? *(const int*)args : 0;
    }

    struct order_log
    {
        int values[16];
        size_t count;
    };

    struct order_entry
    {
        order_log* log;
        int id;
    };

    static void order_handler(void* args, void* cookie)
    {
        order_entry* entry = (order_entry*)cookie;

        if (entry->log->count < std::size(entry->log->values))
        {
            entry->log->values[entry->log->count++] = entry->id;
        }
    }

    struct self_disconnect_state
    {
        ff_signal_connection* connection;
        int calls;
    };

    static void self_disconnect_handler(void* args, void* cookie)
    {
        self_disconnect_state* state = (self_disconnect_state*)cookie;
        state->calls++;
        ff_signal_connection_destroy(state->connection);
    }

    struct disconnect_other_state
    {
        ff_signal_connection* other;
        int calls;
    };

    static void disconnect_other_handler(void* args, void* cookie)
    {
        disconnect_other_state* state = (disconnect_other_state*)cookie;
        state->calls++;
        ff_signal_connection_destroy(state->other);
    }

    struct connect_during_state
    {
        ff_signal* signal;
        ff_signal_connection* added;
        counter* added_state;
        int calls;
    };

    static void connect_during_handler(void* args, void* cookie)
    {
        connect_during_state* state = (connect_during_state*)cookie;
        state->calls++;

        if (state->calls == 1)
        {
            ff_signal_connect(state->signal, state->added, count_handler, state->added_state);
        }
    }

    struct reconnect_state
    {
        ff_signal* signal;
        ff_signal_connection* connection;
        int calls;
    };

    static void reconnect_self_handler(void* args, void* cookie)
    {
        reconnect_state* state = (reconnect_state*)cookie;
        state->calls++;
        ff_signal_connect(state->signal, state->connection, reconnect_self_handler, state);
    }

    struct disconnect_many_state
    {
        ff_signal_connection* connections;
        size_t count;
        int calls;
    };

    static void disconnect_many_handler(void* args, void* cookie)
    {
        disconnect_many_state* state = (disconnect_many_state*)cookie;
        state->calls++;

        for (size_t i = 0; i < state->count; i++)
        {
            ff_signal_connection_destroy(&state->connections[i]);
        }
    }

    struct nested_notify_state
    {
        ff_signal* signal;
        int depth;
        int calls;
    };

    static void nested_notify_handler(void* args, void* cookie)
    {
        nested_notify_state* state = (nested_notify_state*)cookie;
        state->calls++;

        if (state->depth < 3)
        {
            state->depth++;
            ff_signal_notify(state->signal, args);
        }
    }

    struct destroy_signal_state
    {
        ff_signal* signal;
        int calls;
    };

    static void destroy_signal_handler(void* args, void* cookie)
    {
        destroy_signal_state* state = (destroy_signal_state*)cookie;
        state->calls++;
        ff_signal_destroy(state->signal);
    }

    TEST_CLASS(signal_tests)
    {
    public:
        TEST_METHOD(notify_with_no_connections_does_nothing)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            Assert::AreEqual((size_t)0, ff_signal_count(&signal));
            ff_signal_notify(&signal, nullptr);
            Assert::AreEqual((size_t)0, ff_signal_count(&signal));

            ff_signal_destroy(&signal);
        }

        TEST_METHOD(connected_handler_is_called_with_args_and_cookie)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);
            ff_signal_connect(&signal, &connection, count_handler, &state);

            Assert::AreEqual((size_t)1, ff_signal_count(&signal));
            Assert::IsTrue(ff_signal_connection_connected(&connection));

            int value = 42;
            ff_signal_notify(&signal, &value);

            Assert::AreEqual(1, state.calls);
            Assert::AreEqual(42, state.last_value);

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(notify_can_be_repeated)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);
            ff_signal_connect(&signal, &connection, count_handler, &state);

            for (int i = 0; i < 5; i++)
            {
                ff_signal_notify(&signal, &i);
            }

            Assert::AreEqual(5, state.calls);
            Assert::AreEqual(4, state.last_value);

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handlers_are_called_in_connect_order)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            order_log log{};
            ff_signal_connection connections[4];
            order_entry entries[4];

            for (int i = 0; i < 4; i++)
            {
                entries[i].log = &log;
                entries[i].id = i;
                ff_signal_connection_init(&connections[i]);
                ff_signal_connect(&signal, &connections[i], order_handler, &entries[i]);
            }

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual((size_t)4, log.count);

            for (int i = 0; i < 4; i++)
            {
                Assert::AreEqual(i, log.values[i]);
                ff_signal_connection_destroy(&connections[i]);
            }

            ff_signal_destroy(&signal);
        }

        TEST_METHOD(disconnected_handler_stops_being_called)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter first{};
            counter second{};
            ff_signal_connection a;
            ff_signal_connection b;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&b);
            ff_signal_connect(&signal, &a, count_handler, &first);
            ff_signal_connect(&signal, &b, count_handler, &second);

            ff_signal_notify(&signal, nullptr);
            ff_signal_connection_destroy(&a);
            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, first.calls);
            Assert::AreEqual(2, second.calls);
            Assert::AreEqual((size_t)1, ff_signal_count(&signal));
            Assert::IsFalse(ff_signal_connection_connected(&a));

            ff_signal_connection_destroy(&b);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(zeroed_connection_is_safe_to_destroy)
        {
            ff_signal_connection connection{};

            Assert::IsFalse(ff_signal_connection_connected(&connection));
            ff_signal_connection_destroy(&connection);
            Assert::IsFalse(ff_signal_connection_connected(&connection));
        }

        TEST_METHOD(connection_destroy_is_idempotent)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);
            ff_signal_connect(&signal, &connection, count_handler, &state);

            ff_signal_connection_destroy(&connection);
            ff_signal_connection_destroy(&connection);
            ff_signal_connection_destroy(&connection);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(0, state.calls);
            Assert::AreEqual((size_t)0, ff_signal_count(&signal));

            ff_signal_destroy(&signal);
        }

        TEST_METHOD(signal_destroy_is_idempotent)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_destroy(&signal);
            ff_signal_destroy(&signal);

            Assert::AreEqual((size_t)0, ff_signal_count(&signal));
        }

        TEST_METHOD(connection_outliving_the_signal_is_safe_to_destroy)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);
            ff_signal_connect(&signal, &connection, count_handler, &state);

            ff_signal_destroy(&signal);

            Assert::IsFalse(ff_signal_connection_connected(&connection));

            ff_signal_connection_destroy(&connection);
            Assert::AreEqual(0, state.calls);
        }

        TEST_METHOD(many_connections_outliving_the_signal_are_all_inert)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter states[8]{};
            ff_signal_connection connections[8];

            for (int i = 0; i < 8; i++)
            {
                ff_signal_connection_init(&connections[i]);
                ff_signal_connect(&signal, &connections[i], count_handler, &states[i]);
            }

            ff_signal_destroy(&signal);

            for (int i = 0; i < 8; i++)
            {
                Assert::IsFalse(ff_signal_connection_connected(&connections[i]));
                ff_signal_connection_destroy(&connections[i]);
            }
        }

        TEST_METHOD(signal_can_be_reused_after_destroy)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter first{};
            ff_signal_connection a;
            ff_signal_connection_init(&a);
            ff_signal_connect(&signal, &a, count_handler, &first);
            ff_signal_destroy(&signal);

            counter second{};
            ff_signal_connection b;
            ff_signal_connection_init(&b);
            ff_signal_connect(&signal, &b, count_handler, &second);
            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(0, first.calls);
            Assert::AreEqual(1, second.calls);

            ff_signal_connection_destroy(&b);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(connection_can_be_reconnected_after_destroy)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);

            ff_signal_connect(&signal, &connection, count_handler, &state);
            ff_signal_connection_destroy(&connection);
            ff_signal_connect(&signal, &connection, count_handler, &state);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, state.calls);
            Assert::AreEqual((size_t)1, ff_signal_count(&signal));

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(connecting_an_already_connected_connection_moves_it)
        {
            ff_signal first;
            ff_signal second;
            ff_signal_init(&first);
            ff_signal_init(&second);

            counter state{};
            ff_signal_connection connection;
            ff_signal_connection_init(&connection);

            ff_signal_connect(&first, &connection, count_handler, &state);
            ff_signal_connect(&second, &connection, count_handler, &state);

            Assert::AreEqual((size_t)0, ff_signal_count(&first));
            Assert::AreEqual((size_t)1, ff_signal_count(&second));

            ff_signal_notify(&first, nullptr);
            Assert::AreEqual(0, state.calls);

            ff_signal_notify(&second, nullptr);
            Assert::AreEqual(1, state.calls);

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&first);
            ff_signal_destroy(&second);
        }

        TEST_METHOD(one_connection_per_signal_stays_independent)
        {
            ff_signal first;
            ff_signal second;
            ff_signal_init(&first);
            ff_signal_init(&second);

            counter first_state{};
            counter second_state{};
            ff_signal_connection a;
            ff_signal_connection b;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&b);
            ff_signal_connect(&first, &a, count_handler, &first_state);
            ff_signal_connect(&second, &b, count_handler, &second_state);

            ff_signal_notify(&first, nullptr);

            Assert::AreEqual(1, first_state.calls);
            Assert::AreEqual(0, second_state.calls);

            ff_signal_destroy(&first);

            ff_signal_notify(&second, nullptr);
            Assert::AreEqual(1, second_state.calls);

            ff_signal_connection_destroy(&a);
            ff_signal_connection_destroy(&b);
            ff_signal_destroy(&second);
        }

        TEST_METHOD(handler_can_disconnect_itself_during_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection connection;
            ff_signal_connection_init(&connection);

            self_disconnect_state state{};
            state.connection = &connection;
            ff_signal_connect(&signal, &connection, self_disconnect_handler, &state);

            ff_signal_notify(&signal, nullptr);
            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, state.calls);
            Assert::AreEqual((size_t)0, ff_signal_count(&signal));

            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_can_disconnect_the_next_handler_during_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection a;
            ff_signal_connection b;
            ff_signal_connection c;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&b);
            ff_signal_connection_init(&c);

            disconnect_other_state first{};
            first.other = &b;
            counter second{};
            counter third{};

            ff_signal_connect(&signal, &a, disconnect_other_handler, &first);
            ff_signal_connect(&signal, &b, count_handler, &second);
            ff_signal_connect(&signal, &c, count_handler, &third);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, first.calls);
            Assert::AreEqual(0, second.calls);
            Assert::AreEqual(1, third.calls);

            ff_signal_connection_destroy(&a);
            ff_signal_connection_destroy(&b);
            ff_signal_connection_destroy(&c);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_can_disconnect_a_previous_handler_during_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection a;
            ff_signal_connection b;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&b);

            counter first{};
            disconnect_other_state second{};
            second.other = &a;

            ff_signal_connect(&signal, &a, count_handler, &first);
            ff_signal_connect(&signal, &b, disconnect_other_handler, &second);

            ff_signal_notify(&signal, nullptr);
            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, first.calls);
            Assert::AreEqual(2, second.calls);

            ff_signal_connection_destroy(&a);
            ff_signal_connection_destroy(&b);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_connecting_during_notify_is_not_called_until_next_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection a;
            ff_signal_connection added;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&added);

            counter added_state{};
            connect_during_state state{};
            state.signal = &signal;
            state.added = &added;
            state.added_state = &added_state;

            ff_signal_connect(&signal, &a, connect_during_handler, &state);

            ff_signal_notify(&signal, nullptr);
            Assert::AreEqual(1, state.calls);
            Assert::AreEqual(0, added_state.calls);

            ff_signal_notify(&signal, nullptr);
            Assert::AreEqual(2, state.calls);
            Assert::AreEqual(1, added_state.calls);

            ff_signal_connection_destroy(&a);
            ff_signal_connection_destroy(&added);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_reconnecting_itself_during_notify_is_not_called_again)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection connection;
            ff_signal_connection_init(&connection);

            reconnect_state state{};
            state.signal = &signal;
            state.connection = &connection;
            ff_signal_connect(&signal, &connection, reconnect_self_handler, &state);

            ff_signal_notify(&signal, nullptr);
            Assert::AreEqual(1, state.calls);

            ff_signal_notify(&signal, nullptr);
            Assert::AreEqual(2, state.calls);

            Assert::AreEqual((size_t)1, ff_signal_count(&signal));

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_disconnecting_every_connection_during_notify_stops_the_walk)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection connections[3];

            for (int i = 0; i < 3; i++)
            {
                ff_signal_connection_init(&connections[i]);
            }

            counter first{};
            disconnect_many_state second{};
            second.connections = connections;
            second.count = 3;
            counter third{};

            ff_signal_connect(&signal, &connections[0], count_handler, &first);
            ff_signal_connect(&signal, &connections[1], disconnect_many_handler, &second);
            ff_signal_connect(&signal, &connections[2], count_handler, &third);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, first.calls);
            Assert::AreEqual(1, second.calls);
            Assert::AreEqual(0, third.calls);
            Assert::AreEqual((size_t)0, ff_signal_count(&signal));

            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_can_notify_the_same_signal_recursively)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection connection;
            ff_signal_connection_init(&connection);

            nested_notify_state state{};
            state.signal = &signal;
            ff_signal_connect(&signal, &connection, nested_notify_handler, &state);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(4, state.calls);

            ff_signal_connection_destroy(&connection);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(handler_can_destroy_the_whole_signal_during_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection a;
            ff_signal_connection b;
            ff_signal_connection_init(&a);
            ff_signal_connection_init(&b);

            destroy_signal_state first{};
            first.signal = &signal;
            counter second{};

            ff_signal_connect(&signal, &a, destroy_signal_handler, &first);
            ff_signal_connect(&signal, &b, count_handler, &second);

            ff_signal_notify(&signal, nullptr);

            Assert::AreEqual(1, first.calls);
            Assert::AreEqual(0, second.calls);
            Assert::AreEqual((size_t)0, ff_signal_count(&signal));
            Assert::IsFalse(ff_signal_connection_connected(&a));
            Assert::IsFalse(ff_signal_connection_connected(&b));

            ff_signal_connection_destroy(&a);
            ff_signal_connection_destroy(&b);
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(disconnecting_every_handler_during_notify_is_safe)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            ff_signal_connection connections[4];
            self_disconnect_state states[4]{};

            for (int i = 0; i < 4; i++)
            {
                ff_signal_connection_init(&connections[i]);
                states[i].connection = &connections[i];
                ff_signal_connect(&signal, &connections[i], self_disconnect_handler, &states[i]);
            }

            ff_signal_notify(&signal, nullptr);

            for (int i = 0; i < 4; i++)
            {
                Assert::AreEqual(1, states[i].calls);
                Assert::IsFalse(ff_signal_connection_connected(&connections[i]));
            }

            Assert::AreEqual((size_t)0, ff_signal_count(&signal));
            ff_signal_destroy(&signal);
        }

        TEST_METHOD(many_connections_all_receive_the_notify)
        {
            ff_signal signal;
            ff_signal_init(&signal);

            constexpr int count = 200;
            static ff_signal_connection connections[count];
            static counter states[count];

            for (int i = 0; i < count; i++)
            {
                states[i] = counter{};
                ff_signal_connection_init(&connections[i]);
                ff_signal_connect(&signal, &connections[i], count_handler, &states[i]);
            }

            Assert::AreEqual((size_t)count, ff_signal_count(&signal));

            int value = 7;
            ff_signal_notify(&signal, &value);

            for (int i = 0; i < count; i++)
            {
                Assert::AreEqual(1, states[i].calls);
                Assert::AreEqual(7, states[i].last_value);
            }

            ff_signal_destroy(&signal);
        }
    };
}
