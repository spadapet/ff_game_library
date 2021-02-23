#pragma once

namespace ff::filesystem
{
    std::filesystem::path to_path(std::string_view path);
    std::string to_string(const std::filesystem::path& path);
    std::filesystem::path temp_directory_path();
    std::filesystem::path user_directory_path();
    std::filesystem::path to_lower(const std::filesystem::path& path);
    std::filesystem::path clean_file_name(const std::filesystem::path& path);
}
