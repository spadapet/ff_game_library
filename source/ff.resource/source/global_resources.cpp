#include "pch.h"
#include "resource_load.h"
#include "resource_objects.h"

static std::vector<std::shared_ptr<ff::data_base>> global_resource_datas;
static std::shared_ptr<ff::resource_objects> global_resources;
static std::mutex global_resources_mutex;
static ff::signal<> rebuilt_global_signal;

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
    std::scoped_lock lock(::global_resources_mutex);
    ::global_resource_datas.clear();
    ::global_resources.reset();
}

void ff::global_resources::rebuild_async()
{
    std::vector<std::shared_ptr<ff::data_base>> datas;
    {
        std::scoped_lock lock(::global_resources_mutex);
        datas = ::global_resource_datas;
    }

    ff::thread_pool::get()->add_task([datas]()
        {
            std::shared_ptr<ff::resource_objects> global_resources = ::create_global_resources(datas, true);

            ff::thread_dispatch::get_game()->post([global_resources]()
                {
                    if (global_resources)
                    {
                        std::scoped_lock lock(::global_resources_mutex);
                        ::global_resources = global_resources;
                    }

                    ::rebuilt_global_signal.notify();
                });
        });
}

ff::signal_sink<>& ff::global_resources::rebuilt_sink()
{
    return ::rebuilt_global_signal;
}
