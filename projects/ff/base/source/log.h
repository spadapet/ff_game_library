#pragma once

namespace ff::log
{
    void file(std::ostream* file_stream);

    void write(std::string_view text);
    void write_debug(std::string_view text);
    void write_debug(std::ostringstream& str);
}
