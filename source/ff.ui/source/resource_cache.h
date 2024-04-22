#pragma once

namespace ff::internal::ui
{
    /// <summary>
    /// Caches any accessed resource for a short period of time. This prevents reloading the same
    /// resource data over and over again.
    /// </summary>
    class resource_cache : public ff::resource_object_provider
    {
    public:
        resource_cache(std::shared_ptr<ff::resource_object_provider> resources);

        void advance();

        // resource_object_provider
        virtual std::shared_ptr<ff::resource> get_resource_object(std::string_view name) override;
        virtual std::vector<std::string_view> resource_object_names() const override;

    private:
        struct entry_t
        {
            std::unique_ptr<std::string> name;
            std::shared_ptr<ff::resource> resource;
            size_t counter;
        };

        void on_resource_rebuild();

        std::shared_ptr<ff::resource_object_provider> resources;
        std::unordered_map<std::string_view, entry_t> cache;
        ff::signal_connection resource_rebuild_connection;
    };
}
