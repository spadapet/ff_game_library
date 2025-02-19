#include "pch.h"
#include "base/assert.h"
#include "base/string.h"
#include "types/uuid.h"

ff::uuid::uuid(const GUID& other)
    : data(other)
{}

ff::uuid::uuid(std::string_view str)
{
    uuid::data_from_sting(str, this->data);
}

ff::uuid ff::uuid::create()
{
    GUID data;
    return SUCCEEDED(::CoCreateGuid(&data)) ? ff::uuid(data) : ff::uuid::null();
}

const ff::uuid& ff::uuid::null()
{
    static const ff::uuid uuid_null = GUID_NULL;
    return uuid_null;
}

ff::uuid& ff::uuid::operator=(const GUID& other)
{
    this->data = other;
    return *this;
}

ff::uuid& ff::uuid::operator=(std::string_view other)
{
    ff::uuid::data_from_sting(other, this->data);
    return *this;
}

bool ff::uuid::operator==(const uuid& other) const
{
    return this->data == other.data;
}

bool ff::uuid::operator==(const GUID& other) const
{
    return this->data == other;
}

bool ff::uuid::operator!=(const uuid& other) const
{
    return this->data != other.data;
}

bool ff::uuid::operator!=(const GUID& other) const
{
    return this->data != other;
}

bool ff::uuid::operator<(const uuid& other) const
{
    return std::memcmp(&this->data, &other.data, sizeof(GUID)) < 0;
}

bool ff::uuid::operator<=(const uuid& other) const
{
    return std::memcmp(&this->data, &other.data, sizeof(GUID)) <= 0;
}

bool ff::uuid::operator>(const uuid& other) const
{
    return std::memcmp(&this->data, &other.data, sizeof(GUID)) > 0;
}

bool ff::uuid::operator>=(const uuid& other) const
{
    return std::memcmp(&this->data, &other.data, sizeof(GUID)) >= 0;
}

bool ff::uuid::operator!() const
{
    return this->data == GUID_NULL;
}

ff::uuid::operator bool() const
{
    return this->data != GUID_NULL;
}

ff::uuid::operator GUID() const
{
    return this->data;
}

std::string ff::uuid::to_string() const
{
    const size_t size_plus_null = 39;
    std::array<wchar_t, size_plus_null> wstr;
    size_t size = static_cast<size_t>(::StringFromGUID2(this->data, wstr.data(), static_cast<int>(wstr.size())));

    std::string str;
    assert(size == size_plus_null);
    if (size == size_plus_null)
    {
        str = ff::string::to_string(std::wstring_view(wstr.data(), wstr.size() - 1));
    }

    return str;
}

bool ff::uuid::from_string(std::string_view str, uuid& value)
{
    return uuid::data_from_sting(str, value.data);
}

bool ff::uuid::data_from_sting(std::string_view str, GUID& value)
{
    std::wstring wstr = ff::string::to_wstring(str);
    if (SUCCEEDED(::IIDFromString(wstr.data(), &value)))
    {
        return true;
    }

    value = GUID_NULL;
    return false;
}

std::string std::to_string(const ff::uuid& value)
{
    return value.to_string();
}
