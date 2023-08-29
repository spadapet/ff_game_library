#include "pch.h"
#include "resource_load.h"
#include "resource_objects.h"

static std::vector<std::shared_ptr<ff::data_base>> global_resource_datas;
static std::shared_ptr<ff::resource_objects> global_resources;
static std::unique_ptr<ff::co_task<>> global_rebuild_task;
static std::mutex global_rebuild_mutex;
static std::mutex global_resources_mutex;
static ff::signal<> rebuild_begin_signal;
static ff::signal<size_t> rebuild_end_signal;

static std::shared_ptr<ff::resource_objects> create_global_resources(const std::vector<std::shared_ptr<ff::data_base>>& datas, bool from_source)
{
    ff::dict resource_dict;

    for (auto& data : datas)
    {
        ff::dict dict;
        ff::data_reader reader(data);
        if (!ff::dict::load(reader, dict))
        {
            assert(false);
            continue;
        }

        if (from_source)
        {
            std::filesystem::path source_path = ff::filesystem::to_path(dict.get<std::string>(ff::internal::RES_SOURCE));
            if (!source_path.empty())
            {
                ff::load_resources_result result = ff::load_resources_from_file(source_path, true, ff::constants::debug_build);
                assert(result.status);

                if (result.status)
                {
                    dict = std::move(result.dict);
                }
            }
        }

        resource_dict.set(dict, false);
    }

    return std::make_unique<ff::resource_objects>(resource_dict);
}

void ff::global_resources::add(std::shared_ptr<ff::data_base> data)
{
    std::scoped_lock lock(::global_resources_mutex);

    ::global_resource_datas.push_back(data);

    if (::global_resources)
    {
        ff::dict dict;
        ff::data_reader reader(data);
        if (ff::dict::load(reader, dict))
        {
            ::global_resources->add_resources(dict);
        }
    }
}

std::shared_ptr<ff::resource_objects> ff::global_resources::get()
{
    std::scoped_lock lock(::global_resources_mutex);

    if (!::global_resources)
    {
        ::global_resources = ::create_global_resources(::global_resource_datas, false);
    }

    return ::global_resources;
}

std::shared_ptr<ff::resource> ff::global_resources::get(std::string_view name)
{
    return ff::global_resources::get()->get_resource_object(name);
}

void ff::global_resources::reset()
{
    ff::co_task<> task;
    if (::global_rebuild_task)
    {
        std::scoped_lock lock(::global_rebuild_mutex);
        if (::global_rebuild_task)
        {
            task = *::global_rebuild_task;
            ::global_rebuild_task.reset();
        }
    }

    task.wait();

    std::scoped_lock lock(::global_resources_mutex);
    ::global_resource_datas.clear();
    ::global_resources.reset();
}

static ff::co_task<> rebuild_async_internal()
{
    co_await ff::task::yield_on_game();
    ::rebuild_begin_signal.notify();

    std::vector<std::shared_ptr<ff::data_base>> datas;
    {
        std::scoped_lock lock(::global_resources_mutex);
        datas = ::global_resource_datas;
    }

    co_await ff::task::resume_on_task();
    std::shared_ptr<ff::resource_objects> global_resources = ::create_global_resources(datas, true);

    co_await ff::task::resume_on_game();
    if (global_resources)
    {
        std::scoped_lock lock(::global_resources_mutex);
        ::global_resources = global_resources;
    }

    if (::global_rebuild_task)
    {
        std::scoped_lock lock(::global_rebuild_mutex);
        ::global_rebuild_task.reset();
    }

    for (size_t i = 0; i < ff::global_resources::rebuild_round_count; i++)
    {
        ::rebuild_end_signal.notify(i);
    }
}

ff::co_task<> ff::global_resources::rebuild_async()
{
    std::scoped_lock lock(::global_rebuild_mutex);

    if (!::global_rebuild_task || ::global_rebuild_task->done())
    {
        ::global_rebuild_task = std::make_unique<ff::co_task<>>(::rebuild_async_internal());
    }

    return *::global_rebuild_task;
}

bool ff::global_resources::is_rebuilding()
{
    if (::global_rebuild_task)
    {
        std::scoped_lock lock(::global_rebuild_mutex);
        return ::global_rebuild_task && !::global_rebuild_task->done();
    }

    return false;
}

ff::signal_sink<>& ff::global_resources::rebuild_begin_sink()
{
    return ::rebuild_begin_signal;
}

ff::signal_sink<size_t>& ff::global_resources::rebuild_end_sink()
{
    return ::rebuild_end_signal;
}
