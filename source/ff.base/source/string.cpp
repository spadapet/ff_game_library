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

bool ff::string::starts_with(std::string_view str, std::string_view str_start)
{
    return str.size() >= str_start.size() && !str.compare(0, str_start.size(), str_start);
}

bool ff::string::ends_with(std::string_view str, std::string_view str_end)
{
    return str.size() >= str_end.size() && !str.compare(str.size() - str_end.size(), str_end.size(), str_end);
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
