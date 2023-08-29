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
    ff::co_task<> rebuild_async();
    bool is_rebuilding();
    ff::signal_sink<>& rebuild_begin_sink();
    ff::signal_sink<size_t>& rebuild_end_sink();
    constexpr size_t rebuild_round_count = 2;
}
