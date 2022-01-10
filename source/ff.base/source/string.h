#pragma once

namespace ff::string
{
    std::wstring to_wstring(std::string_view str);
    std::string to_string(std::wstring_view wstr);
    std::string from_acp(std::string_view str);
#if UWP_APP
    std::string to_string(Platform::String^ str);
    Platform::String^ to_pstring(std::string_view str);
#endif

    bool starts_with(std::string_view str, std::string_view str_start);
    bool ends_with(std::string_view str, std::string_view str_end);

    std::string to_lower(std::string_view str);
    std::vector<std::string_view> split(std::string_view str, std::string_view delims);
    std::vector<std::string> split_command_line();
    std::vector<std::string> split_command_line(std::string_view str);

    std::string date();
    std::string time();

    template<class... Args>
    std::string concat(Args&&... args)
    {
        std::ostringstream ostr;
        (ostr << ... << args);
        return ostr.str();
    }

    template<class... Args>
    std::wstring concatw(Args&&... args)
    {
        std::ostringstream ostr;
        (ostr << ... << args);
        return ff::string::to_wstring(ostr.str());
    }
}
