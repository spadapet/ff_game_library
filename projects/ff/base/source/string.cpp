#include "pch.h"
#include "string.h"

std::wstring ff::string::to_wstring(std::string_view str)
{
    std::wstring wstr;
    int size = static_cast<int>(str.size());

    if (size)
    {
        int wsize = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), size, nullptr, 0);
        wstr.resize(static_cast<size_t>(wsize));
        ::MultiByteToWideChar(CP_UTF8, 0, str.data(), size, wstr.data(), wsize);
    }

    return wstr;
}

std::string ff::string::to_string(std::wstring_view wstr)
{
    std::string str;
    int wsize = static_cast<int>(wstr.size());

    if (wsize)
    {
        int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wsize, nullptr, 0, nullptr, nullptr);
        str.resize(size);
        ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wsize, str.data(), size, nullptr, nullptr);
    }

    return str;
}

#if UWP_APP

std::string ff::string::to_string(Platform::String^ str)
{
    std::wstring_view wstr(str->Data(), static_cast<size_t>(str->Length()));
    return ff::string::to_string(wstr);
}

#endif
