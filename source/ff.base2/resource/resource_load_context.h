#pragma once

namespace ff
{
    class data_base;

    class resource_load_context
    {
    public:
        virtual const std::filesystem::path& base_path() const = 0;
        virtual const std::vector<std::string>& errors() const = 0;
        virtual void add_error(std::string_view text) = 0;
        virtual void add_output_file(std::string_view name, const std::shared_ptr<ff::data_base>& data) = 0;
        virtual bool debug() const = 0;

        static resource_load_context& null();
    };
}
