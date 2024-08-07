#include "pch.h"
#include "resource_load.h"
#include "resource_objects.h"

static std::shared_ptr<ff::resource_objects> global_resources;
static std::unique_ptr<ff::co_task<>> global_rebuild_task;
static std::mutex global_rebuild_mutex;
static ff::signal<> rebuild_begin_signal;
static ff::signal<ff::push_base<ff::co_task<>>&> rebuild_resources_signal;
static ff::signal<> rebuild_end_signal;

void ff::global_resources::add(ff::reader_base& reader)
{
    ::global_resources->add_resources(reader);
}

bool ff::global_resources::add_files(const std::filesystem::path& path)
{
    return ::global_resources->add_files(path);
}

std::shared_ptr<ff::resource_objects> ff::global_resources::get()
{
    return ::global_resources;
}

std::shared_ptr<ff::resource> ff::global_resources::get(std::string_view name)
{
    return ff::global_resources::get()->get_resource_object(name);
}

void ff::global_resources::destroy_game_thread()
{
    ff::co_task<> task;
    {
        std::scoped_lock lock(::global_rebuild_mutex);
        if (::global_rebuild_task)
        {
            task = *::global_rebuild_task;
            ::global_rebuild_task.reset();
        }
    }

    task.wait();
}

void ff::internal::global_resources::init()
{
    ::global_resources = std::make_shared<ff::resource_objects>();
}

void ff::internal::global_resources::destroy()
{
    ff::global_resources::destroy_game_thread();
    ::global_resources.reset();
}

static ff::co_task<> rebuild_async_internal()
{
    if constexpr (ff::constants::profile_build)
    {
        co_await ff::task::yield_on_game();
        ::rebuild_begin_signal.notify();

        std::vector<ff::co_task<>> rebuild_tasks;
        {
            ff::push_back_collection<std::vector<ff::co_task<>>> push_task(rebuild_tasks);
            ::rebuild_resources_signal.notify(push_task);
        }

        co_await ff::task::resume_on_task();
        for (auto& rebuild_task : rebuild_tasks)
        {
            co_await rebuild_task;
        }

        co_await ff::task::resume_on_game();
        ::rebuild_end_signal.notify();

        std::scoped_lock lock(::global_rebuild_mutex);
        ::global_rebuild_task.reset();
    }
}

ff::co_task<> ff::global_resources::rebuild_async()
{
    if constexpr (ff::constants::profile_build)
    {
        std::scoped_lock lock(::global_rebuild_mutex);

        if (!::global_rebuild_task || ::global_rebuild_task->done())
        {
            ::global_rebuild_task = std::make_unique<ff::co_task<>>(::rebuild_async_internal());
        }

        return *::global_rebuild_task;
    }

    return ff::co_task_source<>::from_result();
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

ff::signal_sink<ff::push_base<ff::co_task<>>&>& ff::global_resources::rebuild_resources_sink()
{
    return ::rebuild_resources_signal;
}

ff::signal_sink<>& ff::global_resources::rebuild_end_sink()
{
    return ::rebuild_end_signal;
}
