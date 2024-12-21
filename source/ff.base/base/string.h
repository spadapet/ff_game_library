#pragma once

namespace ff::string
{
    std::wstring to_wstring(std::string_view str);
    std::string to_string(std::wstring_view wstr);
    std::string from_acp(std::string_view str);
    std::string_view indent_string(size_t spaces);

    bool equals_ignore_case(std::string_view str1, std::string_view str2);

    std::string to_lower(std::string_view str);
    std::vector<std::string_view> split(std::string_view str, std::string_view delims);
    std::vector<std::string> split_command_line();
    std::vector<std::string> split_command_line(std::string_view str);
    void get_module_version_strings(HINSTANCE handle, std::string& out_product_name, std::string& out_internal_name);

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
