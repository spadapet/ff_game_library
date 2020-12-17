#pragma once

namespace ff::internal
{
    class resource_load_context
    {
    public:
        resource_load_context(std::string_view base_path, bool debug);

        const std::filesystem::path& base_path() const;
        const std::vector<std::string> errors() const;
        void add_error(std::string_view text);
        bool debug() const;

    private:
        std::filesystem::path base_path_data;
        std::vector<std::string> errors_data;
        bool debug_data;
    };
}
