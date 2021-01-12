#pragma once

namespace ff
{
    class entity_domain;
    class entity_base;
    using entity = typename entity_base*;
    using const_entity = typename entity_base const*;
    using entity_event_sink = typename ff::signal_sink<ff::entity, size_t, void*>;

    class entity_base
    {
    public:
        entity_base(ff::entity_domain* domain);
        entity_base(entity_base&& other) noexcept = delete;
        entity_base(const entity_base& other) = delete;

        entity_base& operator=(entity_base&& other) noexcept = delete;
        entity_base& operator=(const entity_base& other) = delete;

        ff::entity_domain* domain() const;
        ff::entity clone() const;
        bool active() const;
        void active(bool value);
        void destroy();
        size_t hash() const;
        void event_notify(size_t event_id, void* event_args = nullptr);
        ff::entity_event_sink& event_sink(ff::entity entity, size_t event_id);

        template<class T, class... Args>
        T* set(Args&&... args)
        {
            return this->domain()->set<T, Args...>(this, std::forward<Args>(args)...);
        }

        template<class T>
        T* get() const
        {
            return this->domain()->get<T>(this);
        }

        template<class T>
        bool remove()
        {
            return this->domain()->remove<T>(this);
        }

    private:
        ff::entity_domain* domain_;
    };

    template<>
    struct hash<ff::entity>
    {
        size_t operator()(ff::entity value) const noexcept
        {
            return value->hash();
        }
    };

    class weak_entity
    {
    public:
        weak_entity();
        weak_entity(ff::entity entity);
        weak_entity(const weak_entity& other);
        weak_entity(weak_entity&& other) noexcept;
        ~weak_entity();

        weak_entity& operator=(ff::entity entity);
        weak_entity& operator=(const weak_entity& other);
        weak_entity& operator=(weak_entity&& other) noexcept;

        ff::entity operator->() const;
        operator ff::entity() const;
        operator bool() const;
        bool operator!() const;

        bool operator==(ff::entity entity) const;
        bool operator==(const weak_entity entity) const;

        void reset();
        ff::entity entity() const;

    private:
        void entity_destroyed();

        ff::entity entity_;
        ff::signal_connection destroy_connection;
    };
}
