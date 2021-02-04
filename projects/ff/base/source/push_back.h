#pragma once

namespace ff
{
    template<class T>
    class push_base
    {
    public:
        virtual ~push_base() = default;

        virtual void push(const T& value) const = 0;
        virtual void push(T&& value) const = 0;
    };

    template<class T>
    class push_back_collection: public push_base<typename T::value_type>
    {
    public:
        using value_type = typename T::value_type;

        push_back_collection(T& collection)
            : collection(collection)
        {}

        virtual void push(const value_type& value) const override
        {
            this->collection.push_back(value);
        }

        virtual void push(value_type&& value) const override
        {
            this->collection.push_back(std::move(value));
        }

    private:
        T& collection;
    };
}
