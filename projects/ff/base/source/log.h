#pragma once

namespace ff::log
{
    void write(std::string_view text);
    void write_debug(std::string_view text);
    void write_debug(std::ostringstream& str);
}
