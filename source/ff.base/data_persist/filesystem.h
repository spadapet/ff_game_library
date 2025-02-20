#pragma once

namespace ff
{
    class data_base;
}

namespace ff::filesystem
{
    bool exists(const std::filesystem::path& path);
    bool equivalent(const std::filesystem::path& lhs, const std::filesystem::path& rhs);
    std::filesystem::file_time_type last_write_time(const std::filesystem::path& path);
    bool create_directories(const std::filesystem::path& path);
    size_t file_size(const std::filesystem::path& path);
    bool remove(const std::filesystem::path& path);
    bool remove_all(const std::filesystem::path& path);
    std::filesystem::path weakly_canonical(const std::filesystem::path& path);

    std::string to_string(const std::filesystem::path& path);
    std::string extension_lower_string(const std::filesystem::path& path); // with '.' before extension
    std::filesystem::path to_path(std::string_view path);
    std::filesystem::path module_path(HINSTANCE module);
    std::filesystem::path executable_path();
    std::filesystem::path temp_directory_path();
    std::filesystem::path user_local_path();
    std::filesystem::path user_roaming_path();
    std::filesystem::path to_lower(const std::filesystem::path& path);
    std::filesystem::path clean_file_name(const std::filesystem::path& path);

    std::shared_ptr<ff::data_base> read_binary_file(const std::filesystem::path& path);
    std::shared_ptr<ff::data_base> map_binary_file(const std::filesystem::path& path);
    bool read_text_file(const std::filesystem::path& path, std::string& text);
    bool write_binary_file(const std::filesystem::path& path, const void* data, size_t size);
    bool write_text_file(const std::filesystem::path& path, std::string_view text);
}
