#include "pch.h"
#include "log.h"

void ff::log::write(std::string_view format, ...)
{
    va_list args;
    va_start(args, format);

    std::array<char, 1024> sz;
    _vsnprintf_s(sz.data(), sz.size(), _TRUNCATE, std::string(format).c_str(), args);

    va_end(args);
}
