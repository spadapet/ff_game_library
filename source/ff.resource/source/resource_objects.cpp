#include "pch.h"
#include "resource.h"
#include "resource_load.h"
#include "resource_load_context.h"
#include "resource_object_base.h"
#include "resource_object_v.h"
#include "resource_objects.h"
#include "resource_v.h"
#include "resource_value_provider.h"

using namespace std::string_view_literals;

static const size_t RESOURCE_PERSIST_COOKIE = ff::stable_hash_func("ff::resource_objects@0"sv);
static const size_t RESOURCE_PERSIST_HEADER = ff::stable_hash_func("ff::resource_objects::header@0"sv);
static const size_t RESOURCE_PERSIST_METADATA = ff::stable_hash_func("ff::resource_objects::metadata@0"sv);
static const size_t RESOURCE_PERSIST_DATA = ff::stable_hash_func("ff::resource_objects::data@0"sv);

static ff::value_ptr load_typed_value(std::shared_ptr<ff::saved_data_base> saved_data)
{
    assert_ret_val(saved_data, nullptr);

    auto reader = saved_data->loaded_reader();
    assert_ret_val(reader, nullptr);

    auto value = ff::value::load_typed(*reader);
    assert_ret_val(value, nullptr);

    return value;
}

ff::resource_objects::resource_objects()
    : loading_count(0)
    , done_loading_event(true)
    , rebuild_connection(ff::global_resources::rebuild_resources_sink().connect(std::bind(&ff::resource_objects::rebuild, this, std::placeholders::_1)))
    , resource_metadata_saved(std::make_unique<std::vector<std::shared_ptr<ff::saved_data_base>>>())
    , resource_metadata_dict(std::make_unique<ff::dict>())
{}

ff::resource_objects::resource_objects(const ff::dict& dict)
    : resource_objects()
{
    this->add_resources(dict);
}

ff::resource_objects::resource_objects(ff::reader_base& reader)
    : resource_objects()
{
    this->add_resources(reader);
}

ff::resource_objects::resource_objects(const ff::resource_objects& other)
    : resource_objects()
{
    this->add_resources(other);
}

ff::resource_objects::~resource_objects()
{
    this->flush_all_resources();
}

void ff::resource_objects::add_resources(const ff::dict& dict)
{
    std::scoped_lock lock(this->resource_mutex);
    this->add_resources_only(dict);
    this->add_metadata_only(dict);
}

void ff::resource_objects::add_resources(const ff::resource_objects& other)
{
    std::scoped_lock lock(this->resource_mutex, other.resource_mutex);
    this->add_resources(other.resource_metadata());

    for (auto& [name, other_info] : other.resource_infos)
    {
        this->try_add_resource(name, other_info.saved_value);
    }
}

bool ff::resource_objects::add_resources(ff::reader_base& reader)
{
    std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>> resource_datas;
    std::shared_ptr<ff::saved_data_base> metadata_saved;

    size_t cookie;
    assert_ret_val(ff::load(reader, cookie) && cookie == ::RESOURCE_PERSIST_COOKIE, false);

    // Read header and metadata
    {
        size_t size, data_offset, data_saved_size, data_loaded_size, data_flags;
        assert_ret_val(ff::load(reader, cookie) && cookie == ::RESOURCE_PERSIST_HEADER && ff::load(reader, size), false);
        resource_datas.reserve(size);

        for (size_t i = 0; i < size; i++)
        {
            std::string name;
            assert_ret_val(
                ff::load(reader, name) &&
                ff::load(reader, data_offset) &&
                ff::load(reader, data_saved_size) &&
                ff::load(reader, data_loaded_size) &&
                ff::load(reader, data_flags), false);
            resource_datas.push_back(std::make_tuple(std::move(name), data_offset, data_saved_size, data_loaded_size, data_flags));
        }

        assert_ret_val(
            ff::load(reader, cookie) && cookie == ::RESOURCE_PERSIST_METADATA &&
            ff::load(reader, data_saved_size) &&
            ff::load(reader, data_loaded_size) &&
            ff::load(reader, data_flags), false);

        ff::saved_data_type data_type = static_cast<ff::saved_data_type>(data_flags & 0xFF);
        metadata_saved = reader.saved_data(reader.pos(), data_saved_size, data_loaded_size, data_type);

        const size_t data_cookie_pos = reader.pos() + data_saved_size + ff::save_padding_size(data_saved_size);
        reader.pos(data_cookie_pos);
    }

    std::scoped_lock lock(this->resource_mutex);
    this->resource_metadata_saved->push_back(metadata_saved);

    assert_ret_val(ff::load(reader, cookie) && cookie == ::RESOURCE_PERSIST_DATA, false);
    const size_t data_start = reader.pos();

    for (auto& [name, data_offset, data_saved_size, data_loaded_size, data_flags] : resource_datas)
    {
        ff::saved_data_type data_type = static_cast<ff::saved_data_type>(data_flags & 0xFF);
        auto data = reader.saved_data(data_start + data_offset, data_saved_size, data_loaded_size, data_type);
        this->try_add_resource(name, data);
    }

    return true;
}

// caller must own resource_mutex
void ff::resource_objects::add_resources_only(const ff::dict& dict)
{
    for (auto& [child_name, child_value] : dict)
    {
        if (!child_name.starts_with(ff::internal::RES_PREFIX) && !this->resource_infos.contains(child_name))
        {
            auto data_vector = std::make_shared<std::vector<uint8_t>>();
            ff::data_writer data_writer(data_vector);

            if (child_value->save_typed(data_writer))
            {
                auto data = std::make_shared<ff::data_vector>(data_vector);
                auto saved_data = std::make_shared<ff::saved_data_static>(data, data->size(), ff::saved_data_type::none);
                this->try_add_resource(child_name, saved_data);
            }
        }
    }
}

// caller must own resource_mutex
void ff::resource_objects::add_metadata_only(const ff::dict& dict) const
{
    for (auto& [child_name, child_value] : dict)
    {
        if (child_name == ff::internal::RES_FILES || child_name == ff::internal::RES_SOURCES || child_name == ff::internal::RES_NAMESPACES)
        {
            std::vector<std::string> add_strings = child_value->get<std::vector<std::string>>();
            std::vector<std::string> strings = this->resource_metadata_dict->get<std::vector<std::string>>(child_name);

            for (const std::string& add_string : add_strings)
            {
                if (std::find(strings.cbegin(), strings.cend(), add_string) == strings.cend())
                {
                    strings.push_back(add_string);
                }
            }

            std::sort(strings.begin(), strings.end());
            this->resource_metadata_dict->set<std::vector<std::string>>(child_name, std::move(strings));
        }
        else if (child_name == ff::internal::RES_SOURCE || child_name == ff::internal::RES_NAMESPACE)
        {
            std::string_view multi_child_name = (child_name == ff::internal::RES_SOURCE) ? ff::internal::RES_SOURCES : ff::internal::RES_NAMESPACES;
            std::string add_string = child_value->get<std::string>();
            std::vector<std::string> strings = this->resource_metadata_dict->get<std::vector<std::string>>(multi_child_name);

            if (std::find(strings.cbegin(), strings.cend(), add_string) == strings.cend())
            {
                strings.push_back(std::move(add_string));
                this->resource_metadata_dict->set<std::vector<std::string>>(multi_child_name, std::move(strings));
            }
        }
        else if (child_name == ff::internal::RES_ID_SYMBOLS || child_name == ff::internal::RES_OUTPUT_FILES)
        {
            ff::dict add_dict = child_value->get<ff::dict>();
            ff::dict old_dict = this->resource_metadata_dict->get<ff::dict>(child_name);
            old_dict.set(add_dict, false);
            this->resource_metadata_dict->set<ff::dict>(child_name, std::move(old_dict));
        }
    }
}

// caller must own resource_mutex
bool ff::resource_objects::try_add_resource(std::string_view name, std::shared_ptr<ff::saved_data_base> data)
{
    ff::resource_objects::resource_object_info info{ std::make_unique<std::string>(name), std::move(data) };
    if (!this->resource_infos.try_emplace(*info.name, std::move(info)).second)
    {
        ff::log::write(ff::log::type::resource_load, "Duplicate resource: ", name);
        return false;
    }

    return true;
}

// caller must own resource_mutex
ff::dict& ff::resource_objects::resource_metadata() const
{
    if (!this->resource_metadata_saved->empty())
    {
        for (auto& saved_data : *this->resource_metadata_saved)
        {
            ff::value_ptr value = ::load_typed_value(saved_data)->try_convert<ff::dict>();
            if (value->is_type<ff::dict>())
            {
                this->add_metadata_only(value->get<ff::dict>());
            }
        }

        this->resource_metadata_saved->clear();
    }

    return *this->resource_metadata_dict;
}

bool ff::resource_objects::save(ff::writer_base& writer) const
{
    // Collect the memory for each resource
    std::vector<std::tuple<std::string_view, std::shared_ptr<ff::saved_data_base>>> resource_datas;
    auto metadata_vector = std::make_shared<std::vector<uint8_t>>();
    auto metadata_data = std::make_shared<ff::data_vector>(metadata_vector);
    size_t full_size_guess = sizeof(size_t) * 8; // cookies and sizes
    {
        std::scoped_lock lock(this->resource_mutex);
        resource_datas.reserve(this->resource_infos.size());

        ff::data_writer metadata_writer(metadata_vector);
        ff::value_ptr dict_value = ff::value::create<ff::dict>(ff::dict(this->resource_metadata()));
        assert_ret_val(dict_value->save_typed(metadata_writer), false);
        full_size_guess += metadata_data->size();

        for (auto& [name, info] : this->resource_infos)
        {
            resource_datas.push_back(std::make_tuple(name, info.saved_value));
            full_size_guess += name.size() + info.saved_value->saved_size() + sizeof(size_t) * 6;
        }
    }

    writer.reserve(full_size_guess);
    assert_ret_val(ff::save(writer, ::RESOURCE_PERSIST_COOKIE), false);

    // Write header
    {
        const size_t size = resource_datas.size();
        assert_ret_val(ff::save(writer, ::RESOURCE_PERSIST_HEADER) && ff::save(writer, size), false);

        size_t data_offset = 0;
        for (auto& [name, saved_data] : resource_datas)
        {
            const size_t saved_size = saved_data->saved_size();
            const size_t loaded_size = saved_data->loaded_size();
            const size_t saved_type = static_cast<size_t>(saved_data->type());
            assert_ret_val(
                ff::save(writer, name) &&
                ff::save(writer, data_offset) &&
                ff::save(writer, saved_size) &&
                ff::save(writer, loaded_size) &&
                ff::save(writer, saved_type), false);
            data_offset += saved_size + ff::save_padding_size(saved_size);
        }
    }

    // Write metadata
    {
        const size_t size = metadata_data->size(), data_flags = 0;
        assert_ret_val(ff::save(writer, ::RESOURCE_PERSIST_METADATA) &&
            ff::save(writer, size) &&
            ff::save(writer, size) &&
            ff::save(writer, data_flags), false);
        assert_ret_val(ff::save_bytes(writer, *metadata_data), false);
    }

    // Write data
    {
        assert_ret_val(ff::save(writer, ::RESOURCE_PERSIST_DATA), false);

        for (auto& [name, saved_data] : resource_datas)
        {
            auto data = saved_data->saved_data();
            assert_ret_val(data && ff::save_bytes(writer, *data), false);
        }
    }

    return true;
}

bool ff::resource_objects::save(ff::dict& dict) const
{
    std::scoped_lock lock(this->resource_mutex);
    dict.set(this->resource_metadata(), false);

    for (auto& [name, info] : this->resource_infos)
    {
        ff::value_ptr dict_value = ::load_typed_value(info.saved_value);
        assert_ret_val(dict_value, false);
        dict.set(name, dict_value);
    }

    return true;
}

std::vector<std::string> ff::resource_objects::input_files() const
{
    std::scoped_lock lock(this->resource_mutex);
    return this->resource_metadata().get<std::vector<std::string>>(ff::internal::RES_FILES);
}

std::vector<std::string> ff::resource_objects::source_files() const
{
    std::scoped_lock lock(this->resource_mutex);
    return this->resource_metadata().get<std::vector<std::string>>(ff::internal::RES_SOURCES);
}

std::vector<std::string> ff::resource_objects::source_namespaces() const
{
    std::scoped_lock lock(this->resource_mutex);
    return this->resource_metadata().get<std::vector<std::string>>(ff::internal::RES_NAMESPACES);
}

std::vector<std::pair<std::string, std::string>> ff::resource_objects::id_to_names() const
{
    std::scoped_lock lock(this->resource_mutex);
    std::vector<std::pair<std::string, std::string>> result;
    const ff::dict& dict = this->resource_metadata().get<ff::dict>(ff::internal::RES_ID_SYMBOLS);

    for (auto& [id, name] : dict)
    {
        result.push_back(std::make_pair(std::string(id), name->get<std::string>()));
    }

    std::sort(result.begin(), result.end(),
        [](const auto& l, const auto& r)
        {
            return l.first < r.first;
        });

    return result;
}

std::vector<std::pair<std::string, std::shared_ptr<ff::data_base>>> ff::resource_objects::output_files() const
{
    std::scoped_lock lock(this->resource_mutex);
    std::vector<std::pair<std::string, std::shared_ptr<ff::data_base>>> result;
    const ff::dict& dict = this->resource_metadata().get<ff::dict>(ff::internal::RES_OUTPUT_FILES);

    for (auto& [id, data] : dict)
    {
        result.push_back(std::make_pair(std::string(id), data->get<ff::data_base>()));
    }

    return result;
}

std::shared_ptr<ff::resource> ff::resource_objects::get_resource_object(std::string_view name)
{
    std::shared_ptr<ff::resource> value;
    {
        std::scoped_lock lock(this->resource_mutex);
        value = this->get_resource_object_here(name);
    }

    if (!value)
    {
        value = std::make_shared<ff::resource>(name, ff::value::create<nullptr_t>());
    }

    return value;
}

// Caller must own the this->resource_object_info_mutex lock
std::shared_ptr<ff::resource> ff::resource_objects::get_resource_object_here(std::string_view name)
{
    std::shared_ptr<ff::resource> resource_result;

    auto iter = this->resource_infos.find(name);
    if (iter != this->resource_infos.cend())
    {
        ff::resource_objects::resource_object_info& info = iter->second;
        resource_result = info.weak_value.lock();

        if (!resource_result)
        {
            if (this->loading_count.fetch_add(1) == 0)
            {
                this->done_loading_event.reset();
            }

            resource_result = std::make_shared<ff::resource>(name);

            auto loading_info = std::make_shared<ff::resource_objects::resource_object_loading_info>();
            loading_info->loading_resource = resource_result;
            loading_info->name = name;
            loading_info->owner = &info;
            loading_info->start_time = ff::timer::current_raw_time();
            loading_info->blocked_count = 1;

            info.weak_value = resource_result;
            info.weak_loading_info = loading_info;

            ff::log::write(ff::log::type::resource_load, "Loading: ", name);

            ff::thread_pool::add_task([this, loading_info]()
            {
                ff::value_ptr dict_value = ::load_typed_value(loading_info->owner->saved_value);
                ff::value_ptr new_value = this->create_resource_objects(loading_info, dict_value);
                this->update_resource_object_info(loading_info, new_value);
                // no code here since the destructor may be running
            });
        }
    }

    return resource_result;
}

std::vector<std::string_view> ff::resource_objects::resource_object_names() const
{
    std::scoped_lock lock(this->resource_mutex);
    std::vector<std::string_view> names;
    names.reserve(this->resource_infos.size());

    for (auto& i : this->resource_infos)
    {
        names.push_back(i.first);
    }

    return names;
}

void ff::resource_objects::flush_all_resources()
{
    this->done_loading_event.wait();
}

ff::co_task<> ff::resource_objects::flush_all_resources_async()
{
    co_await this->done_loading_event;
}

void ff::resource_objects::update_resource_object_info(std::shared_ptr<ff::resource_objects::resource_object_loading_info> loading_info, ff::value_ptr new_value)
{
    std::unique_lock loading_lock(loading_info->mutex);

    assert(loading_info->blocked_count > 0);
    bool loading_done = loading_info->blocked_count > 0 && !--loading_info->blocked_count;

    ff::log::write(ff::log::type::resource_load, "Update: ", loading_info->name, " (", loading_info->blocked_count, (loading_done ? ", done)" : ", blocked)"));

    if (loading_done)
    {
        std::shared_ptr<ff::resource_object_base> new_obj = new_value->get<ff::resource_object_base>();
        if (new_obj && !new_obj->resource_load_complete(false))
        {
            assert(false);
            new_value = ff::value::create<nullptr_t>();
        }
    }

    loading_info->final_value = new_value;

    if (loading_done)
    {
        loading_info->loading_resource->finalize_value(new_value);
        {
            std::scoped_lock lock(this->resource_mutex);
            loading_info->owner->weak_loading_info.reset();
        }

        for (auto& parent_loading_info : loading_info->parent_loading_infos)
        {
            ff::log::write(ff::log::type::resource_load, "Unblocking: '", parent_loading_info->name, "' unblocked by '", loading_info->name, "'");

            this->update_resource_object_info(parent_loading_info, parent_loading_info->final_value);
        }

        const double seconds = ff::timer::seconds_since_raw(loading_info->start_time);
        ff::log::write(ff::log::type::resource, "Loaded: ", loading_info->name, " (", &std::fixed, std::setprecision(1), seconds * 1000.0, "ms)");
    }

    loading_lock.unlock();

    if (loading_done && this->loading_count.fetch_sub(1) == 1)
    {
        this->done_loading_event.set();
    }
}

ff::value_ptr ff::resource_objects::create_resource_objects(std::shared_ptr<ff::resource_objects::resource_object_loading_info> loading_info, ff::value_ptr value)
{
    if (!value)
    {
        return value;
    }

    ff::value_ptr dict_value = ff::type::try_get_dict_from_data(value);
    if (dict_value)
    {
        // Convert dicts with a "res:type" value to a ff::resource_object_base
        ff::dict dict = dict_value->get<ff::dict>();
        std::string type = dict.get<std::string>(ff::internal::RES_TYPE);
        const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(type);

        std::vector<std::string_view> names = dict.child_names();
        for (std::string_view name : names)
        {
            ff::value_ptr new_value = this->create_resource_objects(loading_info, dict.get(name));
            dict.set(name, new_value);
        }

        if (factory)
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
            vec[i] = this->create_resource_objects(loading_info, vec[i]);
        }

        value = ff::value::create<std::vector<ff::value_ptr>>(std::move(vec));
    }
    else if (value->is_type<std::string>() || value->is_type<ff::resource>())
    {
        // Resolve references to other resources (and update existing references with the latest value)
        ff::value_ptr string_val = value->convert_or_default<std::string>();
        std::string_view str = string_val->get<std::string>();

        if (str.starts_with(ff::internal::REF_PREFIX))
        {
            std::string_view ref_name = str.substr(ff::internal::REF_PREFIX.size());
            std::shared_ptr<ff::resource> ref_value = this->get_resource_object(ref_name);
            {
                std::shared_ptr<ff::resource_objects::resource_object_loading_info> ref_loading_info;
                {
                    std::scoped_lock lock(this->resource_mutex);
                    auto i = this->resource_infos.find(ref_name);
                    if (i != this->resource_infos.cend())
                    {
                        ref_loading_info = i->second.weak_loading_info.lock();
                    }
                }

                if (ref_loading_info)
                {
                    std::scoped_lock locks(loading_info->mutex, ref_loading_info->mutex);

                    if (ref_loading_info->blocked_count)
                    {
                        ff::log::write(ff::log::type::resource_load, "Blocking: '", loading_info->name, "' blocked by '", ref_loading_info->name, "'");

                        loading_info->blocked_count++;
                        ref_loading_info->parent_loading_infos.push_back(loading_info);
                    }
                }
            }

            value = ff::value::create<ff::resource>(ref_value);
        }
        else if (str.starts_with(ff::internal::LOC_PREFIX))
        {
            std::string_view loc_name = str.substr(ff::internal::LOC_PREFIX.size());
            value = nullptr; // this->get_localized_value(loc_name);

            if (!value)
            {
                ff::log::write_debug_fail(ff::log::type::resource, "Missing localized resource value: ", loc_name);
            }
        }
    }
    else if (value->is_type<ff::saved_data_base>())
    {
        // fully load and uncompress all saved data
        value = this->create_resource_objects(loading_info, value->try_convert<ff::data_base>());
    }

    return value;
}

void ff::resource_objects::rebuild(ff::push_base<ff::co_task<>>& tasks)
{
    tasks.push(this->rebuild_async());
}

ff::co_task<> ff::resource_objects::rebuild_async()
{
    if constexpr (!ff::constants::profile_build)
    {
        co_return;
    }

    co_await ff::task::yield_on_task();
    co_await this->flush_all_resources_async();

    std::scoped_lock lock(this->resource_mutex);
    std::unordered_map<std::string_view, std::shared_ptr<ff::resource>> old_resources;

    for (auto& [name, info] : this->resource_infos)
    {
        std::shared_ptr<ff::resource> old_resource = info.weak_value.lock();
        if (old_resource)
        {
            old_resources.try_emplace(name, std::move(old_resource));
        }
    }

    this->resource_infos.clear();

    for (const std::filesystem::path& source_path : this->source_files())
    {
        ff::load_resources_result result = ff::load_resources_from_file(source_path, true, ff::constants::profile_build);
        if (result.resources)
        {
            this->add_resources(*result.resources);
        }
    }

    for (auto& [name, old_resource] : old_resources)
    {
        std::shared_ptr<ff::resource> new_resource = this->get_resource_object_here(name);
        if (new_resource)
        {
            old_resource->new_resource(new_resource);
        }
    }
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

    if (result.resources)
    {
        return std::make_shared<ff::resource_objects>(*result.resources);
    }

    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::resource_objects_factory::load_from_cache(const ff::dict& dict) const
{
    std::shared_ptr<ff::saved_data_base> saved_data = dict.get<ff::saved_data_base>("resources");
    assert_ret_val(saved_data, nullptr);

    auto reader = saved_data->loaded_reader();
    assert_ret_val(reader, nullptr);

    return std::make_shared<ff::resource_objects>(*reader);
}

bool ff::resource_objects::save_to_cache(ff::dict& dict) const
{
    std::shared_ptr<ff::saved_data_base> saved_data;
    {
        auto data_vector = std::make_shared<std::vector<uint8_t>>();
        ff::data_writer writer(data_vector);
        assert_ret_val(this->save(writer), false);

        auto data = std::make_shared<ff::data_vector>(data_vector);
        saved_data = std::make_shared<ff::saved_data_static>(data, data->size(), ff::saved_data_type::none);
    }

    dict.set<ff::saved_data_base>("resources", std::move(saved_data));
    return true;
}
