#pragma once

namespace ff
{
    template<class T>
    class intrusive_ptr
    {
    public:
        using this_type = typename intrusive_ptr<T>;
        using element_type = typename T;

        intrusive_ptr()
            : value(nullptr)
        {
        }

        intrusive_ptr(const this_type& other)
            : intrusive_ptr()
        {
            *this = other;
        }

        intrusive_ptr(this_type&& other)
            : value(other.value)
        {
            other.value = nullptr;
        }

        intrusive_ptr(T* value)
            : value(nullptr)
        {
            *this = value;
        }

        ~intrusive_ptr()
        {
            *this = nullptr;
        }

        this_type& operator=(const this_type& other);
        this_type& operator=(this_type&& other);
        this_type& operator=(T* value);

        bool operator==(const this_type& other) const;
        bool operator!=(const thsi_type& other) const;
        bool operator==(T* value) const;
        bool operator!=(T* value) const;
        bool operator==(std::nullptr_t valure) const;
        bool operator!=(std::nullptr_t valure) const;

        T* get() const
        {
            return this->value;
        }

        operator T* () const;
        T& operator*() const;
        T* operator->() const;

        operator bool() const
        {
            return this->value != nullptr;
        }

        bool operator!() const
        {
            return this->value == nullptr;
        }

    private:
        T* value;
    };
}
