#pragma once

namespace ff
{
    class signal_sink_base;

    class signal_connection
    {
    public:
        struct entry_t
        {
            signal_connection* connection;
            signal_sink_base* sink;
        };

        signal_connection() = default;
        signal_connection(entry_t* entry);
        signal_connection(signal_connection&& other) noexcept;
        signal_connection(const signal_connection& other) = delete;
        ~signal_connection();

        signal_connection& operator=(signal_connection&& other) noexcept;
        signal_connection& operator=(const signal_connection& other) = delete;
        operator bool() const;

        void disconnect();

    private:
        void connect(entry_t* entry);

        entry_t* entry{};
    };

    class signal_sink_base
    {
    public:
        virtual ~signal_sink_base() = default;
        virtual void disconnecting(ff::signal_connection::entry_t* entry) = 0;
    };

    template<class... Args>
    class signal_sink : public ff::signal_sink_base
    {
    public:
        ff::signal_connection connect(std::function<void(Args...)>&& func)
        {
            if (this->disconnected_count && !this->notify_nest_count)
            {
                for (auto& i : this->handlers)
                {
                    if (!i.connection)
                    {
                        this->disconnected_count--;
                        i.handler = std::move(func);
                        return ff::signal_connection(&i);
                    }
                }
            }

            return ff::signal_connection(&this->handlers.emplace_front(this, std::move(func)));
        }

        virtual void disconnecting(ff::signal_connection::entry_t* entry) override
        {
            handler_entry_t* handler_entry = static_cast<handler_entry_t*>(entry);
            handler_entry->connection = nullptr;
            handler_entry->handler = {};
            this->disconnected_count++;
        }

    protected:
        struct handler_entry_t : public ff::signal_connection::entry_t
        {
            handler_entry_t(ff::signal_sink_base* sink, std::function<void(Args...)>&& handler)
                : ff::signal_connection::entry_t{ nullptr, sink }
                , handler(std::move(handler))
            {}

            std::function<void(Args...)> handler;
        };

        std::forward_list<handler_entry_t> handlers;
        int notify_nest_count{};
        int disconnected_count{};
    };

    template<class... Args>
    class signal : public signal_sink<Args...>
    {
    public:
        virtual ~signal() override
        {
            for (auto& i : this->handlers)
            {
                if (i.connection)
                {
                    i.connection->disconnect();
                }
            }
        }

        void notify(Args... args)
        {
            bool saw_empty = false;
            this->notify_nest_count++;

            for (auto& i : this->handlers)
            {
                if (i.connection)
                {
                    i.handler(args...);
                }
                else
                {
                    saw_empty = true;
                }
            }

            if (!--this->notify_nest_count && saw_empty)
            {
                for (auto prev = this->handlers.cbefore_begin(), i = this->handlers.cbegin(); i != this->handlers.cend(); prev = i++)
                {
                    if (!i->connection)
                    {
                        this->handlers.erase_after(prev);
                        i = prev;
                    }
                }
            }
        }
    };
}
