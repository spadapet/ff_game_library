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

static std::string get_version_string(const uint8_t* version_bytes, std::string_view key)
{
    std::wstring full_key = ff::string::concatw("\\StringFileInfo\\040904b0\\", key);
    wchar_t* value = nullptr;
    UINT value_size = 0;

    if (::VerQueryValue(version_bytes, full_key.c_str(), reinterpret_cast<void**>(&value), &value_size) && value_size > 1)
    {
        return ff::string::to_string(std::wstring_view(value, static_cast<size_t>(value_size) - 1));
    }

    return {};
}

ff::module_version_t ff::string::get_module_version(HINSTANCE module)
{
    ff::module_version_t result;
    std::filesystem::path exe_path = ff::filesystem::module_path(module);
    std::wstring wexe_path = exe_path.native();

    DWORD version_handle, version_size;
    if ((version_size = ::GetFileVersionInfoSize(wexe_path.c_str(), &version_handle)) != 0)
    {
        std::vector<uint8_t> version_bytes;
        version_bytes.resize(static_cast<size_t>(version_size));

        if (::GetFileVersionInfo(wexe_path.c_str(), 0, version_size, version_bytes.data()))
        {
            result.product_name = ::get_version_string(version_bytes.data(), "ProductName");
            result.internal_name = ::get_version_string(version_bytes.data(), "InternalName");
            result.company_name = ::get_version_string(version_bytes.data(), "CompanyName");
            result.product_version = ::get_version_string(version_bytes.data(), "ProductVersion");
            result.file_version = ::get_version_string(version_bytes.data(), "FileVersion");
            result.copyright = ::get_version_string(version_bytes.data(), "LegalCopyright");
        }
    }

    // Fallbacks

    if (result.product_name.empty() && result.internal_name.size())
    {
        result.product_name = result.internal_name;
    }

    if (result.product_name.empty())
    {
        result.product_name = ff::filesystem::to_string(exe_path.stem());
    }

    if (result.internal_name.empty())
    {
        result.internal_name = result.product_name;
    }

    return result;
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
