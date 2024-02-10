#pragma once
#include "global_resources.h"
#include "resource_object_base.h"
#include "resource_object_provider.h"
#include "resource_object_factory_base.h"

namespace ff
{
    class resource_value_provider;
}

namespace ff::internal
{
    extern std::string_view RES_FACTORY_NAME;
}

namespace ff
{
    class resource_objects
        : public ff::resource_object_base
        , public ff::resource_object_loader
    {
    public:
        resource_objects(const ff::dict& dict);
        resource_objects(resource_objects&& other) noexcept = delete;
        resource_objects(const resource_objects& other) = delete;
        ~resource_objects();

        resource_objects& operator=(resource_objects&& other) noexcept = delete;
        resource_objects& operator=(const resource_objects& other) = delete;

        virtual std::shared_ptr<ff::resource> get_resource_object(std::string_view name) override;
        virtual std::vector<std::string_view> resource_object_names() const override;
        virtual void flush_all_resources() override;

        void add_object_provider(std::shared_ptr<ff::resource_object_provider> value);
        void add_value_provider(std::shared_ptr<ff::resource_value_provider> value);

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        friend void ff::global_resources::add(std::shared_ptr<ff::data_base> data);

        struct resource_object_info;

        struct resource_object_loading_info
        {
            std::recursive_mutex mutex;
            std::shared_ptr<ff::resource> loading_resource;
            ff::value_ptr final_value;
            std::vector<std::shared_ptr<resource_object_loading_info>> parent_loading_infos;
            std::string name;
            resource_object_info* owner;
            int blocked_count;
        };

        struct resource_object_info
        {
            ff::value_ptr dict_value;
            std::weak_ptr<ff::resource> weak_value;
            std::weak_ptr<resource_object_loading_info> weak_loading_info;
        };

        void add_resources(const ff::dict& dict);
        void update_resource_object_info(std::shared_ptr<resource_object_loading_info> loading_info, ff::value_ptr new_value);
        ff::value_ptr create_resource_objects(std::shared_ptr<resource_object_loading_info> loading_info, ff::value_ptr value);
        ff::value_ptr get_localized_value(std::string_view name);
        std::shared_ptr<ff::resource> get_resource_object_here(std::string_view name);

        mutable std::recursive_mutex resource_object_info_mutex;
        std::unordered_map<std::string_view, resource_object_info> resource_object_infos;
        std::vector<std::shared_ptr<ff::resource_object_provider>> object_providers;
        std::vector<std::shared_ptr<ff::resource_value_provider>> value_providers;
        ff::win_event done_loading_event;
        std::atomic<int> loading_count;
    };
}

namespace ff::internal
{
    class resource_objects_factory : public ff::resource_object_factory<resource_objects>
    {
    public:
        using ff::resource_object_factory<resource_objects>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
