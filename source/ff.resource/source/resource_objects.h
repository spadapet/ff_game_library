#pragma once
#include "global_resources.h"
#include "resource_object_base.h"
#include "resource_object_provider.h"
#include "resource_object_factory_base.h"

namespace ff
{
    class resource_value_provider;
}

namespace ff
{
    class resource_objects
        : public ff::resource_object_base
        , public ff::resource_object_loader
    {
    public:
        resource_objects();
        resource_objects(const ff::dict& dict);
        resource_objects(ff::reader_base& reader);
        resource_objects(const ff::resource_objects& other);
        resource_objects(ff::resource_objects&& other) noexcept = delete;
        ~resource_objects();

        ff::resource_objects& operator=(const ff::resource_objects& other) = delete;
        ff::resource_objects& operator=(ff::resource_objects&& other) noexcept = delete;

        // Load/Save
        void add_resources(const ff::dict& dict);
        void add_resources(const ff::resource_objects& other);
        bool add_resources(ff::reader_base& reader);
        bool save(ff::writer_base& writer) const;
        bool save(ff::dict& dict) const;

        // Metadata
        std::vector<std::string> input_files() const;
        std::vector<std::string> source_files() const;
        std::vector<std::string> source_namespaces() const;
        std::vector<std::pair<std::string, std::string>> id_to_names() const;
        std::vector<std::pair<std::string, std::shared_ptr<ff::data_base>>> output_files() const;

        // ff::resource_object_loader
        virtual std::shared_ptr<ff::resource> get_resource_object(std::string_view name) override;
        virtual std::vector<std::string_view> resource_object_names() const override;
        virtual void flush_all_resources() override;
        virtual ff::co_task<> flush_all_resources_async() override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        void add_resources_only(const ff::dict& dict);
        void add_metadata_only(const ff::dict& dict) const;
        bool try_add_resource(std::string_view name, std::shared_ptr<ff::saved_data_base> data);
        ff::dict& resource_metadata() const; // must be holding resource_mutex
        void rebuild(ff::push_base<ff::co_task<>>& tasks);
        ff::co_task<> rebuild_async();

        struct resource_object_info;

        struct resource_object_loading_info
        {
            std::recursive_mutex mutex;
            std::shared_ptr<ff::resource> loading_resource;
            ff::value_ptr final_value;
            std::vector<std::shared_ptr<ff::resource_objects::resource_object_loading_info>> parent_loading_infos;
            std::string name;
            ff::resource_objects::resource_object_info* owner;
            int64_t start_time;
            int blocked_count;
        };

        struct resource_object_info
        {
            std::unique_ptr<std::string> name;
            std::shared_ptr<ff::saved_data_base> saved_value;
            std::weak_ptr<ff::resource> weak_value;
            std::weak_ptr<ff::resource_objects::resource_object_loading_info> weak_loading_info;
        };

        void update_resource_object_info(std::shared_ptr<ff::resource_objects::resource_object_loading_info> loading_info, ff::value_ptr new_value);
        ff::value_ptr create_resource_objects(std::shared_ptr<ff::resource_objects::resource_object_loading_info> loading_info, ff::value_ptr value);
        std::shared_ptr<ff::resource> get_resource_object_here(std::string_view name);

        mutable std::recursive_mutex resource_mutex;
        std::unique_ptr<std::vector<std::shared_ptr<ff::saved_data_base>>> resource_metadata_saved;
        std::unique_ptr<ff::dict> resource_metadata_dict;
        std::unordered_map<std::string_view, ff::resource_objects::resource_object_info> resource_infos;

        std::atomic<int> loading_count;
        ff::win_event done_loading_event;
        ff::signal_connection rebuild_connection;
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
