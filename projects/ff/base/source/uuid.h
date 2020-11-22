#pragma once

namespace ff
{
    class uuid
    {
    public:
        uuid() = default;
        uuid(const uuid& other) = default;
        uuid(const GUID& other);
        uuid(std::string_view str);

        static uuid create();
        static const uuid& null();

        uuid& operator=(const uuid& other) = default;
        uuid& operator=(const GUID& other);
        uuid& operator=(std::string_view other);

        bool operator==(const uuid& other) const;
        bool operator==(const GUID& other) const;
        bool operator!=(const uuid& other) const;
        bool operator!=(const GUID& other) const;

        bool operator<(const uuid& other) const;
        bool operator<=(const uuid& other) const;
        bool operator>(const uuid& other) const;
        bool operator>=(const uuid& other) const;

        bool operator!() const;
        operator bool() const;
        operator GUID() const;

        void swap(uuid& other);
        std::string to_string() const;

    private:
        static GUID data_from_sting(std::string_view str);

        GUID data;
    };
}

namespace std
{
    void swap(ff::uuid& left, ff::uuid& right);
    std::string to_string(const ff::uuid& value);
}
