#include "pch.h"
#include "log.h"
#include "string.h"

static std::ostream* file_stream;

void ff::log::file(std::ostream* file_stream)
{
    ::file_stream = file_stream;
}

void ff::log::write(std::string_view text)
{
    std::ostringstream str;
    str << ff::string::date() << ' ' << ff::string::time() << ": " << text;

    std::string str2 = str.str();

    if (::file_stream)
    {
        *::file_stream << str2 << std::endl;
    }

    std::cerr << str2 << std::endl;

    ff::log::write_debug(str);
}

void ff::log::write_debug(std::string_view text)
{
#ifdef _DEBUG
    std::wostringstream str;
    str << ff::string::to_wstring(text) << std::endl;
    ::OutputDebugString(str.str().c_str());
#endif
}

void ff::log::write_debug(std::ostringstream& str)
{
#ifdef _DEBUG
    str << std::endl;
    ::OutputDebugString(ff::string::to_wstring(str.str()).c_str());
#endif
}
