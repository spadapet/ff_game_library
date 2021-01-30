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

    std::vector<std::string_view> split(std::string_view str, std::string_view delims);
}
