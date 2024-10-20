#pragma once

#include "../resource/resource_object_base.h"
#include "../resource/resource_object_factory_base.h"

namespace ff
{
    class resource_file : public ff::resource_object_base
    {
    public:
        resource_file(std::string_view file_extension, HINSTANCE instance, const wchar_t* rc_type, const wchar_t* rc_name);
        resource_file(const std::filesystem::path& path);
        resource_file(std::shared_ptr<ff::saved_data_base> saved_data, std::string_view file_extension);

        std::shared_ptr<ff::data_base> loaded_data() const;
        const std::shared_ptr<ff::saved_data_base>& saved_data() const;
        const std::string& file_extension() const;

        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        std::shared_ptr<ff::saved_data_base> saved_data_;
        std::string file_extension_;
    };
}

namespace ff::internal
{
    class resource_file_factory : public ff::resource_object_factory<resource_file>
    {
    public:
        using ff::resource_object_factory<resource_file>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
