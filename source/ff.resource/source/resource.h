#pragma once

namespace ff
{
    class resource_object_loader;

    class resource
    {
    public:
        resource(std::string_view name); // still loading
        resource(std::string_view name, ff::value_ptr value);
        resource(resource&& other) noexcept = default;
        resource(const resource& other) = delete;

        resource& operator=(resource&& other) noexcept = default;
        resource& operator=(const resource& other) = delete;

        std::string_view name() const;
        ff::value_ptr value(bool force = false) const;
        ff::co_task<ff::value_ptr> value_async() const;
        void finalize_value(ff::value_ptr value);

    private:
        std::string name_;
        ff::value_ptr value_;
        std::atomic<std::shared_ptr<ff::win_event>> finalized_event;
    };
}
