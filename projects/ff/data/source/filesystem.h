#pragma once

namespace ff
{
    class data_base;
}

namespace ff::filesystem
{
    std::shared_ptr<ff::data_base> read_binary_file(const std::filesystem::path& path);
    bool read_text_file(const std::filesystem::path& path, std::string& text);
    bool write_text_file(const std::filesystem::path& path, std::string_view text);
}
