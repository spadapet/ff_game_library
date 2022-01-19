#include "pch.h"
#include "resource_load_context.h"

ff::resource_load_context& ff::resource_load_context::null()
{
    class null_context : public ff::resource_load_context
    {
    public:
        virtual const std::filesystem::path& base_path() const
        {
            return this->base_path_;
        }

        virtual const std::vector<std::string>& errors() const
        {
            return this->errors_;
        }

        virtual void add_error(std::string_view text)
        {}

        virtual void add_output_file(std::string_view name, const std::shared_ptr<ff::data_base>& data) override
        {}

        virtual bool debug() const
        {
            return false;
        }

    private:
        std::filesystem::path base_path_;
        std::vector<std::string> errors_;
    };

    static null_context null_value;
    return null_value;
}
