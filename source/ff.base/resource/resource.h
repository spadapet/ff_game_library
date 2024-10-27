#pragma once

#include "../data_value/value_ptr.h"
#include "../thread/co_task.h"

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
        ff::value_ptr value() const;
        ff::co_task<ff::value_ptr> value_async() const;
        bool is_loading() const;
        void finalize_value(ff::value_ptr value);
        void new_resource(std::shared_ptr<ff::resource> value);
        std::shared_ptr<ff::resource> new_resource() const;

    private:
        std::string name_;
        ff::value_ptr value_;
        std::atomic<std::shared_ptr<ff::win_event>> finalized_event;
        std::atomic<std::shared_ptr<ff::resource>> new_resource_;
        bool finalized_flag{};
        bool new_resource_flag{};
    };
}
