#pragma once

namespace ff
{
    class resource;
    class resource_objects;
}

namespace ff::global_resources
{
    void add(std::shared_ptr<ff::data_base> data);
    std::shared_ptr<ff::resource_objects> get();
    std::shared_ptr<ff::resource> get(std::string_view name);

    void reset();
    void rebuild_async();
    ff::signal_sink<void>& rebuilt_sink();
}
