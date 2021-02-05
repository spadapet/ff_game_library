#include "pch.h"
#include "resource.h"
#include "resource_load.h"
#include "resource_load_context.h"
#include "resource_object_base.h"
#include "resource_object_v.h"
#include "resource_objects.h"
#include "resource_v.h"
#include "resource_value_provider.h"

std::string_view ff::internal::RES_FACTORY_NAME("resource_objects");

ff::resource_objects::resource_objects(const ff::dict& dict)
    : localized_value_provider_(nullptr)
    , done_loading_event(ff::create_event(true))
    , loading_count(0)
{
    for (std::string_view name : dict.child_names())
    {
        if (!ff::string::starts_with(name, ff::internal::RES_PREFIX))
        {
            resource_object_info info;
            info.dict_value = dict.get(name);
            this->resource_object_infos.try_emplace(name, std::move(info));
        }
    }
}

ff::resource_objects::~resource_objects()
{
    this->flush_all_resources();
}

const ff::resource_object_factory_base* ff::resource_objects::factory()
{
    return ff::resource_object_base::get_factory(ff::internal::RES_FACTORY_NAME);
}

static std::shared_ptr<ff::resource> create_null_resource(std::string_view name, ff::resource_object_loader* loading_owner = nullptr)
{
    ff::value_ptr null_value = ff::value::create<nullptr_t>();
    return std::make_shared<ff::resource>(name, null_value, loading_owner);
}

std::shared_ptr<ff::resource> ff::resource_objects::get_resource_object(std::string_view name)
{
    std::shared_ptr<ff::resource> value;

    auto iter = this->resource_object_infos.find(name);
    if (iter != this->resource_object_infos.cend())
    {
        std::lock_guard lock(this->mutex);
        resource_object_info& info = iter->second;
        value = info.weak_value.lock();

        if (!value)
        {
            if (++this->loading_count == 1)
            {
                ::ResetEvent(this->done_loading_event);
            }

            value = ::create_null_resource(name, this);

            info.weak_value = value;
            info.loading_info = std::make_unique<resource_object_loading_info>();
            info.loading_info->original_value = value;
            info.loading_info->event = ff::create_event();

            std::string keep_name(name);
            ff::thread_pool::get()->add_task([this, keep_name, &info]()
                {
                    ff::value_ptr new_value = this->create_resource_objects(info, info.dict_value);
                    this->update_resource_object_info(info, std::make_shared<ff::resource>(keep_name, new_value));
                    // no code here since the destructor may be running
                });
        }
    }

    return value ? value : ::create_null_resource(name);
}

std::vector<std::string_view> ff::resource_objects::resource_object_names() const
{
    std::vector<std::string_view> names;
    names.reserve(this->resource_object_infos.size());

    for (auto& i : this->resource_object_infos)
    {
        names.push_back(i.first);
    }

    return names;
}

std::shared_ptr<ff::resource> ff::resource_objects::flush_resource(const std::shared_ptr<ff::resource>& value)
{
    ff::resource_object_loader* owner = value->loading_owner();
    if (owner == this)
    {
        ff::timer timer;
        ff::win_handle load_event;
        {
            std::lock_guard lock(this->mutex);

            auto i = this->resource_object_infos.find(value->name());
            if (i != this->resource_object_infos.cend())
            {
                resource_object_info& info = i->second;
                if (info.loading_info)
                {
                    load_event = info.loading_info->event.duplicate();
                }
            }
        }

        if (load_event)
        {
            ff::wait_for_handle(load_event);

            double seconds = timer.tick();

#ifdef _DEBUG
            std::ostringstream str;
            str << "[ff/res] Blocked on resource: " << value->name() << " (" << std::setprecision(4) << seconds * 1000.0 << "ms)";
            ff::log::write_debug(str);
#endif
        }
    }

    std::shared_ptr<ff::resource> new_resource = value->new_resource();
    return new_resource ? new_resource : value;
}

void ff::resource_objects::flush_all_resources()
{
    ff::wait_for_handle(this->done_loading_event);
    assert(!this->loading_count);
}

const ff::resource_value_provider* ff::resource_objects::localized_value_provider() const
{
    return this->localized_value_provider_;
}

void ff::resource_objects::localized_value_provider(const resource_value_provider* value)
{
    this->localized_value_provider_ = value;
}

bool ff::resource_objects::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    for (auto& i : this->resource_object_infos)
    {
        dict.set(i.first, i.second.dict_value);
    }

    return true;
}

void ff::resource_objects::update_resource_object_info(resource_object_info& info, std::shared_ptr<ff::resource> new_value)
{
    // notify new object that loading is done
    if (new_value)
    {
        this->mutex.lock();
        bool done_loading = info.loading_info->child_infos.empty();
        this->mutex.unlock();

        std::shared_ptr<ff::resource_object_base> new_obj = new_value->value()->get<ff::resource_object_base>();
        if (done_loading && new_obj && !new_obj->resource_load_complete(false))
        {
            assert(false);
            new_value = ::create_null_resource(new_value->name());
        }
    }

    std::lock_guard lock(this->mutex);
    std::vector<resource_object_info*> parent_infos;

    info.loading_info->final_value = new_value;

    if (info.loading_info->child_infos.empty())
    {
        std::shared_ptr<ff::resource> old_resource = info.weak_value.lock();
        if (old_resource)
        {
            old_resource->new_resource(new_value);
        }

        info.weak_value = new_value;

        ff::win_handle event = std::move(info.loading_info->event);
        std::swap(parent_infos, info.loading_info->parent_infos);
        info.loading_info.reset();

        ::SetEvent(event);
    }

    for (resource_object_info* parent_info : parent_infos)
    {
        resource_object_loading_info& parent_loading_info = *parent_info->loading_info;
        auto iter = std::find(parent_loading_info.child_infos.cbegin(), parent_loading_info.child_infos.cend(), &info);
        if (iter != parent_loading_info.child_infos.cend())
        {
            parent_loading_info.child_infos.erase(iter);
        }

        if (parent_loading_info.child_infos.empty() && parent_loading_info.final_value)
        {
            this->update_resource_object_info(*parent_info, parent_loading_info.final_value);
        }
    }

    if (!info.loading_info)
    {
        assert(this->loading_count > 0);
        if (this->loading_count && !--this->loading_count)
        {
            ::SetEvent(this->done_loading_event);
        }
    }
}

ff::value_ptr ff::resource_objects::create_resource_objects(resource_object_info& info, ff::value_ptr value)
{
    if (!value)
    {
        return value;
    }

    ff::value_ptr dict_value = ff::type::try_get_dict_from_data(value);
    if (dict_value)
    {
        // Convert dicts with a "res:type" value to a COM object
        ff::dict dict = dict_value->get<ff::dict>();
        std::string type = dict.get<std::string>(ff::internal::RES_TYPE);
        const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(type);
        bool is_nested_resources = (type == ff::internal::RES_FACTORY_NAME);

        if (!is_nested_resources)
        {
            std::vector<std::string_view> names = dict.child_names();
            for (std::string_view name : names)
            {
                ff::value_ptr new_value = this->create_resource_objects(info, dict.get(name));
                dict.set(name, new_value);
            }
        }

        if (!is_nested_resources && factory)
        {
            std::shared_ptr<ff::resource_object_base> obj = factory->load_from_cache(dict);
            value = ff::value::create<ff::resource_object_base>(obj);
        }
        else
        {
            value = ff::value::create<ff::dict>(std::move(dict));
        }
    }
    else if (value->is_type<std::vector<ff::value_ptr>>())
    {
        std::vector<ff::value_ptr> vec = value->get<std::vector<ff::value_ptr>>();
        for (size_t i = 0; i < vec.size(); i++)
        {
            vec[i] = this->create_resource_objects(info, vec[i]);
        }

        value = ff::value::create<std::vector<ff::value_ptr>>(std::move(vec));
    }
    else if (value->is_type<std::string>() || value->is_type<ff::resource>())
    {
        // Resolve references to other resources (and update existing references with the latest value)
        ff::value_ptr string_val = value->convert_or_default<std::string>();
        std::string_view str = string_val->get<std::string>();

        if (ff::string::starts_with(str, ff::internal::REF_PREFIX))
        {
            std::string_view ref_name = str.substr(ff::internal::REF_PREFIX.size());
            std::shared_ptr<ff::resource> ref_value = this->get_resource_object(ref_name);
            {
                std::lock_guard lock(this->mutex);
                auto i = this->resource_object_infos.find(ref_name);
                if (i != this->resource_object_infos.cend())
                {
                    resource_object_info& ref_info = i->second;
                    if (ref_info.loading_info)
                    {
                        info.loading_info->child_infos.push_back(&ref_info);
                        ref_info.loading_info->parent_infos.push_back(&info);
                    }
                }
            }

            value = ff::value::create<ff::resource>(ref_value);
        }
        else if (ff::string::starts_with(str, ff::internal::LOC_PREFIX))
        {
            std::string_view loc_name = str.substr(ff::internal::LOC_PREFIX.size());
            value = this->localized_value_provider() ? this->localized_value_provider()->get_resource_value(loc_name) : nullptr;
#ifdef _DEBUG
            if (!value)
            {
                std::ostringstream error;
                error << "Missing localized resource value: " << loc_name;
                ff::log::write_debug(error);
                assert(false);
            }
#endif
        }
    }
    else if (value->is_type<ff::saved_data_base>())
    {
        // fully load and uncompress all saved data
        value = this->create_resource_objects(info, value->try_convert<ff::data_base>());
    }

    return value;
}

std::shared_ptr<ff::resource_object_base> ff::internal::resource_objects_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    std::vector<std::string> errors;
    ff::load_resources_result result = ff::load_resources_from_json(dict, context.base_path(), context.debug());

    if (!result.errors.empty())
    {
        for (const std::string& error : result.errors)
        {
            context.add_error(error);
        }
    }

    return result.status ? this->load_from_cache(result.dict) : nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::resource_objects_factory::load_from_cache(const ff::dict& dict) const
{
    return std::make_shared<resource_objects>(dict);
}
