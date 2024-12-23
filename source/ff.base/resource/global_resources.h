#pragma once

#include "../thread/co_task.h"

namespace ff
{
    class resource;
    class resource_objects;
}

namespace ff::global_resources
{
    void add(ff::reader_base& reader);
    bool add_files(const std::filesystem::path& path);
    std::shared_ptr<ff::resource_objects> get();
    std::shared_ptr<ff::resource> get(std::string_view name);
    void destroy_game_thread();

    ff::co_task<> rebuild_async();
    bool is_rebuilding();
    ff::signal_sink<>& rebuild_begin_sink();
    ff::signal_sink<ff::push_base<ff::co_task<>>&>& rebuild_resources_sink();
    ff::signal_sink<>& rebuild_end_sink();
}

namespace ff::internal::global_resources
{
    void init();
    void destroy();
}
