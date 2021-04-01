#pragma once

namespace ff
{
    class signal_connection
    {
    public:
        typedef void (*disconnect_func)(void*);

        signal_connection();
        signal_connection(disconnect_func func, void* cookie);
        signal_connection(signal_connection&& other) noexcept;
        signal_connection(const signal_connection& other) = delete;
        ~signal_connection();

        signal_connection& operator=(signal_connection&& other) noexcept;
        signal_connection& operator=(const signal_connection& other) = delete;

        void disconnect();

    private:
        disconnect_func func;
        void* cookie;
    };

    template<class... Args>
    class signal_sink
    {
    public:
        using handler_type = typename std::function<void(Args...)>;
        using this_type = signal_sink<Args...>;

        signal_connection connect(handler_type&& func)
        {
            void* cookie = &this->handlers.emplace_front(handler_entry{ std::move(func), false });
            return signal_connection(&this_type::disconnect_func, cookie);
        }

    protected:
        struct handler_entry
        {
            handler_type handler;
            bool disconnected;
        };

        std::forward_list<handler_entry> handlers;

    private:
        static void disconnect_func(void* cookie)
        {
            handler_entry* handler = reinterpret_cast<handler_entry*>(cookie);
            handler->disconnected = true;
        }
    };

    template<>
    class signal_sink<void>
    {
    public:
        using handler_type = typename std::function<void()>;
        using this_type = signal_sink<void>;

        signal_connection connect(handler_type&& func)
        {
            void* cookie = &this->handlers.emplace_front(handler_entry{ std::move(func), false });
            return signal_connection(&this_type::disconnect_func, cookie);
        }

    protected:
        struct handler_entry
        {
            handler_type handler;
            bool disconnected;
        };

        std::forward_list<handler_entry> handlers;

    private:
        static void disconnect_func(void* cookie)
        {
            handler_entry* handler = reinterpret_cast<handler_entry*>(cookie);
            handler->disconnected = true;
        }
    };
}

namespace ff
{
    template<class... Args>
    class signal : public signal_sink<Args...>
    {
    public:
        void notify(Args... args)
        {
            for (auto prev = this->handlers.cbefore_begin(), i = this->handlers.cbegin(); i != this->handlers.cend(); prev = i++)
            {
                if (!i->disconnected)
                {
                    i->handler(args...);
                }

                if (i->disconnected)
                {
                    this->handlers.erase_after(prev);
                    i = prev;
                }
            }
        }
    };

    template<>
    class signal<void> : public signal_sink<void>
    {
    public:
        void notify()
        {
            for (auto prev = this->handlers.cbefore_begin(), i = this->handlers.cbegin(); i != this->handlers.cend(); prev = i++)
            {
                if (!i->disconnected)
                {
                    i->handler();
                }

                if (i->disconnected)
                {
                    this->handlers.erase_after(prev);
                    i = prev;
                }
            }
        }
    };
}
