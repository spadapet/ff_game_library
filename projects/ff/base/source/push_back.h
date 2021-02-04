#pragma once

namespace ff
{
    template<class T>
    class push_back_base
    {
    public:
        virtual ~push_back_base() = 0;

        virtual void push_back(const T& value) const = 0;
        virtual void push_back(T&& value) const = 0;
    };

    template<class T>
    class push_back_collection: public push_back_base<typename T::value_type>
    {
    public:
        using value_type = typename T::value_type;

        push_back_collection(T& collection)
            : collection(collection)
        {}

        virtual void push_back(const value_type& value) const override
        {
            this->collection.push_back(value);
        }

        virtual void push_back(value_type&& value) const override
        {
            this->collection.push_back(std::move(value));
        }

    private:
        T& collection;
    };
}
