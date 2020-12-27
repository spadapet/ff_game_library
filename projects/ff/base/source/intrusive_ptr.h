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
        {}

        intrusive_ptr(const this_type& other)
            : intrusive_ptr()
        {
            *this = other;
        }

        intrusive_ptr(T* value)
            : intrusive_ptr()
        {
            this->reset(value);
        }

        intrusive_ptr(this_type&& other) noexcept
            : intrusive_ptr()
        {
            *this = std::move(other);
        }

        ~intrusive_ptr()
        {
            this->reset();
        }

        this_type& operator=(const this_type& other)
        {
            this->reset(other.value);
            return *this;
        }

        this_type& operator=(this_type&& other) noexcept
        {
            std::swap(this->value, other.value);
            return *this;
        }

        bool operator==(const this_type& other) const
        {
            return this->value == other.value;
        }

        bool operator!=(const this_type& other) const
        {
            return this->value != other.value;
        }

        bool operator==(T* value) const
        {
            return this->value == value;
        }

        bool operator!=(T* value) const
        {
            return this->value != value;
        }

        bool operator==(std::nullptr_t value) const
        {
            return this->value == nullptr;
        }

        bool operator!=(std::nullptr_t value) const
        {
            return this->value != nullptr;
        }

        void reset()
        {
            this->reset(nullptr);
        }

        void reset(T* value)
        {
            if (this->value != value)
            {
                if (value)
                {
                    value->add_ref();
                }

                if (this->value)
                {
                    this->value->release_ref();
                }

                this->value = value;
            }
        }

        T* get() const
        {
            return this->value;
        }

        operator T* () const
        {
            return this->value;
        }

        T& operator*() const
        {
            return *this->value;
        }

        T* operator->() const
        {
            return this->value;
        }

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
