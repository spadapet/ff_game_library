#pragma once

#include "resource_object_base.h"
#include "resource_object_factory_base.h"

namespace ff::object
{
    class file_o : public ff::resource_object_base
    {
    public:
        file_o(std::shared_ptr<ff::saved_data_base> saved_data, std::string_view file_extension, bool compress);
        const std::shared_ptr<ff::saved_data_base>& saved_data() const;

        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        std::shared_ptr<ff::saved_data_base> saved_data_;
        std::string file_extension;
        bool compress;
    };

    class file_factory : public ff::resource_object_factory<file_o>
    {
    public:
        using ff::resource_object_factory<file_o>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
