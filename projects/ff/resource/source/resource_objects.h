#pragma once
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

        static const resource_object_factory_base* factory();
        static void register_global_dict(std::shared_ptr<ff::data_base> data);
        static resource_objects& global();
        static void reset_global();

        resource_objects& operator=(resource_objects&& other) noexcept = delete;
        resource_objects& operator=(const resource_objects& other) = delete;

        virtual std::shared_ptr<ff::resource> get_resource_object(std::string_view name) override;
        virtual std::vector<std::string_view> resource_object_names() const override;
        virtual std::shared_ptr<ff::resource> flush_resource(const std::shared_ptr<ff::resource>& value) override;
        virtual void flush_all_resources() override;

        const resource_value_provider* localized_value_provider() const;
        void localized_value_provider(const resource_value_provider* value);

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        struct resource_object_info;

        struct resource_object_loading_info
        {
            ff::win_handle event;
            std::shared_ptr<ff::resource> original_value;
            std::shared_ptr<ff::resource> final_value;
            std::vector<resource_object_info*> child_infos;
            std::vector<resource_object_info*> parent_infos;
        };

        struct resource_object_info
        {
            std::weak_ptr<ff::resource> weak_value;
            ff::value_ptr dict_value;
            std::unique_ptr<resource_object_loading_info> loading_info;
        };

        void update_resource_object_info(resource_object_info& info, std::shared_ptr<ff::resource> new_value);
        ff::value_ptr create_resource_objects(resource_object_info& info, ff::value_ptr value);

        std::recursive_mutex mutex;
        std::unordered_map<std::string_view, resource_object_info> resource_object_infos;
        ff::resource_value_provider const* localized_value_provider_;
        ff::win_handle done_loading_event;
        size_t loading_count;
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
