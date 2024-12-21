#include "pch.h"
#include "base/string.h"
#include "data_persist/filesystem.h"

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

std::string ff::string::from_acp(std::string_view str)
{
    std::string str8;
    std::wstring wstr;
    int size = static_cast<int>(str.size());

    if (size)
    {
        int wsize = ::MultiByteToWideChar(CP_ACP, 0, str.data(), size, nullptr, 0);
        wstr.resize(static_cast<size_t>(wsize));
        ::MultiByteToWideChar(CP_ACP, 0, str.data(), size, wstr.data(), wsize);

        str8 = ff::string::to_string(wstr);
    }

    return str8;
}

std::string_view ff::string::indent_string(size_t spaces)
{
    static const std::string str(64, ' ');
    return std::string_view(str.begin(), str.begin() + std::min<size_t>(spaces, str.size()));
}

bool ff::string::equals_ignore_case(std::string_view str1, std::string_view str2)
{
    return str1.size() == str2.size() && ::_strnicmp(str1.data(), str2.data(), str1.size()) == 0;
}

std::string ff::string::to_lower(std::string_view str)
{
    std::string result(str);

    result.push_back(0);
    ::_strlwr_s(result.data(), static_cast<DWORD>(result.size()));
    result.pop_back();

    return result;
}

std::vector<std::string_view> ff::string::split(std::string_view str, std::string_view delims)
{
    std::vector<std::string_view> tokens;

    for (size_t pos = 0; pos < str.size(); )
    {
        size_t end = str.find_first_of(delims, pos);
        end = (end == std::string_view::npos) ? str.size() : end;

        if (end > pos)
        {
            tokens.push_back(str.substr(pos, end - pos));
        }

        pos = (end != str.size()) ? str.find_first_not_of(delims, end) : str.size();
        pos = (pos == std::string_view::npos) ? str.size() : pos;
    }

    return tokens;
}

std::vector<std::string> ff::string::split_command_line()
{
    return ff::string::split_command_line(ff::string::to_string(::GetCommandLine()));
}

std::vector<std::string> ff::string::split_command_line(std::string_view str)
{
    std::vector<std::string> tokens;
    std::string token;
    char quote = 0;

    for (char ch : str)
    {
        if (!quote)
        {
            if (std::isspace(ch))
            {
                // end of a token
                if (!token.empty())
                {
                    tokens.push_back(std::move(token));
                }
            }
            else if (ch == '\"' || ch == '\'')
            {
                // start of a string
                quote = ch;
            }
            else
            {
                token += ch;
            }
        }
        else
        {
            // inside of a quoted string

            if (ch == quote)
            {
                // the string has ended
                quote = 0;
            }
            else
            {
                token += ch;
            }
        }
    }

    if (!token.empty())
    {
        // save the last token

        tokens.push_back(std::move(token));
    }

    return tokens;
}

void ff::string::get_module_version_strings(HINSTANCE handle, std::string& out_product_name, std::string& out_internal_name)
{
    std::array<wchar_t, 2048> wpath;
    DWORD size = static_cast<DWORD>(wpath.size());
    if ((size = ::GetModuleFileName(handle, wpath.data(), size)) != 0)
    {
        std::wstring wstr(std::wstring_view(wpath.data(), static_cast<size_t>(size)));

        DWORD handle, version_size;
        if ((version_size = ::GetFileVersionInfoSize(wstr.c_str(), &handle)) != 0)
        {
            std::vector<uint8_t> version_bytes;
            version_bytes.resize(static_cast<size_t>(version_size));

            if (::GetFileVersionInfo(wstr.c_str(), 0, version_size, version_bytes.data()))
            {
                wchar_t* product_name = nullptr;
                UINT product_name_size = 0;

                wchar_t* internal_name = nullptr;
                UINT internal_name_size = 0;

                if (::VerQueryValue(version_bytes.data(), L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&product_name), &product_name_size) && product_name_size > 1)
                {
                    out_product_name = ff::string::to_string(std::wstring_view(product_name, static_cast<size_t>(product_name_size) - 1));
                }

                if (::VerQueryValue(version_bytes.data(), L"\\StringFileInfo\\040904b0\\InternalName", reinterpret_cast<void**>(&internal_name), &internal_name_size) && internal_name_size > 1)
                {
                    out_internal_name = ff::string::to_string(std::wstring_view(internal_name, static_cast<size_t>(internal_name_size) - 1));
                }
            }
        }

        if (out_product_name.empty())
        {
            std::filesystem::path path = ff::filesystem::to_path(ff::string::to_string(wstr));
            out_product_name = ff::filesystem::to_string(path.stem());
        }
    }

    // Fallbacks

    if (out_product_name.empty())
    {
        out_product_name = "App";
    }

    if (out_internal_name.empty())
    {
        out_internal_name = out_product_name;
    }
}

std::string ff::string::date()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    std::ostringstream str;
    str << std::setfill('0') << std::setw(2) << st.wMonth << '/' << std::setw(2) << st.wDay << '/' << std::setw(4) << st.wYear;
    return str.str();
}

std::string ff::string::time()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);

    std::ostringstream str;
    str << std::setfill('0') << std::setw(2) << st.wHour << ':' << std::setw(2) << st.wMinute << ':' << std::setw(2) << st.wSecond;
    return str.str();
}
