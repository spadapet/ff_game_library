#pragma once

namespace ff::filesystem
{
    bool read_text_file(const std::filesystem::path& path, std::string& text);
    bool write_text_file(const std::filesystem::path& path, std::string_view text);
}
