#pragma once

namespace ff::string
{
    std::wstring to_wstring(std::string_view str);
    std::string to_string(std::wstring_view wstr);
#if UWP_APP
    std::string to_string(Platform::String^ str);
#endif
}
