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
            void* cookie = &this->handlers.emplace_front(std::move(func));
            return signal_connection(&this_type::disconnect_func, cookie);
        }

    protected:
        std::forward_list<handler_type> handlers;

    private:
        static void disconnect_func(void* cookie)
        {
            handler_type* handler = reinterpret_cast<handler_type*>(cookie);
            *handler = handler_type();
        }
    };
}

namespace ff::internal
{
    template<class... Args>
    class signal_site : public signal_sink<Args...>
    {
    public:
        void notify(Args... args)
        {
            for (auto prev = this->handlers.cbefore_begin(), i = this->handlers.cbegin(); i != this->handlers.cend(); prev = i++)
            {
                if (*i)
                {
                    (*i)(args...);
                }

                if (!*i)
                {
                    this->handlers.erase_after(prev);
                    i = prev;
                }
            }
        }
    };
}

namespace ff
{
    template<class... Args>
    class signal : public ff::internal::signal_site<Args...>
    {};

    template<>
    class signal<void> : public ff::internal::signal_site<>
    {};
}
