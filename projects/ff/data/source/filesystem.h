#pragma once

namespace ff::filesystem
{
    std::filesystem::path temp_directory_path();
    std::filesystem::path user_directory_path();
    std::filesystem::path to_lower(const std::filesystem::path& path);
    bool read_text_file(const std::filesystem::path& path, std::string& text);
}
