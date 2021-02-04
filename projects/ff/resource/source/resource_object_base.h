#pragma once

namespace ff
{
    class resource;
    class resource_object_factory_base;

    class resource_object_base : public std::enable_shared_from_this<resource_object_base>
    {
    public:
        virtual ~resource_object_base() = default;

        template<class T>
        static bool register_factory(std::string_view name)
        {
            return ff::resource_object_base::register_type(std::make_unique<T>(name));
        }

        static const ff::resource_object_factory_base* get_factory(std::string_view name);
        static const ff::resource_object_factory_base* get_factory(std::type_index type);

        static bool save_to_cache_typed(const resource_object_base& value, ff::dict& dict, bool& allow_compress);
        static std::shared_ptr<resource_object_base> load_from_cache_typed(const ff::dict& dict);

        virtual bool resource_load_complete(bool from_source);
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const;
        virtual ff::dict resource_get_siblings(const std::shared_ptr<resource>& self) const;
        virtual bool resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const = 0;

    private:
        static bool register_type(std::unique_ptr<resource_object_factory_base>&& type);
    };
}
