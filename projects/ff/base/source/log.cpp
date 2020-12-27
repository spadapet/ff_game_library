#include "pch.h"
#include "log.h"
#include "string.h"

void ff::log::write(std::string_view text)
{
#ifdef _DEBUG
    ff::log::write_debug(text);
#endif
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
