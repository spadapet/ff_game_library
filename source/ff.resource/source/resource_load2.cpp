#include "pch.h"
#include "resource.h"
#include "resource_v.h"
#include "resource_load.h"
#include "resource_load_context.h"
#include "resource_object_base.h"
#include "resource_object_factory_base.h"
#include "resource_object_v.h"
#include "resource_objects.h"

static std::string make_symbol_from_name(std::string_view name)
{
    std::ostringstream id;
    bool pending_underscore = false;

    for (char ch : name)
    {
        if (std::isalnum(ch) || ch == '_')
        {
            if (id.tellp() == 0 && std::isdigit(ch))
            {
                pending_underscore = true;
            }

            if (pending_underscore && ch != '_')
            {
                id << '_';
            }

            pending_underscore = false;
            id << static_cast<char>(std::toupper(ch));
        }
        else
        {
            pending_underscore = true;
        }
    }

    return id.str();
}

class transformer_context : public ff::resource_load_context
{
public:
    transformer_context(const std::filesystem::path& base_path, bool debug)
        : base_path_(base_path)
        , debug_(debug)
    {}

    virtual const std::filesystem::path& base_path() const override
    {
        return this->base_path_;
    }

    virtual const std::vector<std::string>& errors() const override
    {
        return this->errors_;
    }

    virtual void add_error(std::string_view text) override
    {
        if (!text.empty())
        {
            std::scoped_lock lock(this->mutex);
            this->errors_.emplace_back(text);
        }
    }

    virtual void add_output_file(std::string_view name, const std::shared_ptr<ff::data_base>& data) override
    {
        if (name.size() && data && data->size())
        {
            std::scoped_lock lock(this->mutex);
            this->output_files_.push_back(std::make_pair(std::string(name), data));
        }
    }

    virtual bool debug() const override
    {
        return this->debug_;
    }

    const ff::dict& values() const
    {
        static ff::dict empty_dict;
        std::scoped_lock lock(this->mutex);
        auto iter = this->thread_to_values.find(::GetCurrentThreadId());
        return iter != this->thread_to_values.cend() ? iter->second.back() : empty_dict;
    }

    ff::dict& push_values()
    {
        std::scoped_lock lock(this->mutex);

        std::vector<ff::dict>& values = this->thread_to_values.try_emplace(::GetCurrentThreadId(), std::vector<ff::dict>()).first->second;

        ff::dict dict;
        if (!values.empty())
        {
            dict = values.back();
        }

        values.push_back(std::move(dict));
        return values.back();
    }

    ff::dict& push_values_from_owner_thread(DWORD owner_thread_id)
    {
        std::scoped_lock lock(this->mutex);

        ff::dict& dict = this->push_values();
        auto iter = this->thread_to_values.find(owner_thread_id);
        if (iter != this->thread_to_values.cend())
        {
            const std::vector<ff::dict>& owner_values = iter->second;
            dict.set(owner_values.back(), false);
        }

        return dict;
    }

    void pop_values()
    {
        std::scoped_lock lock(this->mutex);

        auto iter = this->thread_to_values.find(::GetCurrentThreadId());
        if (iter != this->thread_to_values.cend())
        {
            std::vector<ff::dict>& values = iter->second;
            values.pop_back();

            if (values.empty())
            {
                this->thread_to_values.erase(iter);
            }
        }
    }

    std::shared_ptr<ff::resource> set_reference(std::string_view name_view, ff::value_ptr value = nullptr)
    {
        std::string name(name_view);
        std::scoped_lock lock(this->mutex);

        auto i = this->name_to_resource.find(name);
        if (i != this->name_to_resource.cend())
        {
            if (value)
            {
                i->second->finalize_value(value);
            }
        }
        else
        {
            i = this->name_to_resource.try_emplace(std::move(name), value
                ? std::make_shared<ff::resource>(name_view, value)
                : std::make_shared<ff::resource>(name_view)).first;
        }

        return i->second;
    }

    void finalize_missing_references()
    {
        std::scoped_lock lock(this->mutex);

        for (auto& i : this->name_to_resource)
        {
            if (i.second->is_loading())
            {
                i.second->finalize_value(nullptr);
            }
        }
    }

    void add_file(std::string_view resource_name, const std::filesystem::path& path)
    {
        std::filesystem::path path_canon = std::filesystem::weakly_canonical(path);
        std::scoped_lock lock(this->mutex);
        this->paths_.insert(path_canon);

        if (!resource_name.empty())
        {
            std::string resource_name_str(resource_name);
            auto i = this->name_to_paths.find(resource_name_str);
            if (i == this->name_to_paths.cend())
            {
                i = this->name_to_paths.try_emplace(resource_name_str).first;
            }

            i->second.insert(path_canon);
        }
    }

    std::vector<std::filesystem::path> paths() const
    {
        std::scoped_lock lock(this->mutex);

        std::vector<std::filesystem::path> paths;
        paths.reserve(this->paths_.size());

        for (const std::filesystem::path& path : this->paths_)
        {
            paths.push_back(path);
        }

        return paths;
    }

    std::vector<std::filesystem::path> paths(std::string_view name) const
    {
        std::vector<std::filesystem::path> paths;
        std::string name_str(name);
        std::scoped_lock lock(this->mutex);

        auto i = this->name_to_paths.find(name_str);
        if (i != this->name_to_paths.cend())
        {
            for (const std::filesystem::path& path : i->second)
            {
                paths.push_back(path);
            }
        }

        return paths;
    }

    using output_files_t = typename std::vector<std::pair<std::string, std::shared_ptr<ff::data_base>>>;
    using id_to_name_t = typename std::vector<std::pair<std::string, std::string>>;

    output_files_t output_files() const
    {
        output_files_t output_files_copy;
        {
            std::scoped_lock lock(this->mutex);
            output_files_copy = this->output_files_;
        }

        return output_files_copy;
    }

    void set_id_to_name(std::string_view id, std::string_view name)
    {
        std::scoped_lock lock(this->mutex);
        this->id_to_name_.try_emplace(std::string(id), std::string(name));
    }

    id_to_name_t id_to_name()
    {
        id_to_name_t id_to_name_copy;
        {
            std::scoped_lock lock(this->mutex);
            std::copy(this->id_to_name_.cbegin(), this->id_to_name_.cend(), std::back_inserter(id_to_name_copy));
        }

        return id_to_name_copy;
    }

private:
    mutable std::recursive_mutex mutex;
    std::filesystem::path base_path_;
    std::vector<std::string> errors_;
    output_files_t output_files_;
    std::unordered_map<DWORD, std::vector<ff::dict>> thread_to_values;
    std::unordered_map<std::string, std::shared_ptr<ff::resource>> name_to_resource;
    std::unordered_map<std::string, std::unordered_set<std::filesystem::path>> name_to_paths;
    std::unordered_set<std::filesystem::path, ff::stable_hash<std::filesystem::path>> paths_;
    std::unordered_map<std::string, std::string> id_to_name_;
    bool debug_;
};

class transformer_base : public ff::dict_visitor_base
{
public:
    transformer_base(transformer_context& context)
        : context_(context)
    {}

protected:
    virtual bool async_allowed(const ff::dict& dict) override
    {
        return this->is_root();
    }

    transformer_context& context() const
    {
        return this->context_;
    }

private:
    transformer_context& context_;
};

class expand_file_paths_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_value(ff::value_ptr value) override
    {
        value = transformer_base::transform_value(value);

        if (value && value->is_type<std::string>())
        {
            std::string string_value = value->get<std::string>();
            if (ff::string::starts_with(string_value, ff::internal::FILE_PREFIX))
            {
                string_value = string_value.substr(ff::internal::FILE_PREFIX.size());
                std::filesystem::path path_value = string_value;
                std::filesystem::path full_path = std::filesystem::weakly_canonical(this->context().base_path() / path_value);

                if (ff::filesystem::exists(full_path))
                {
                    std::string name = this->path_root_name();
                    this->context().add_file(name, full_path);
                }
                else
                {
                    std::ostringstream str;
                    str << "File not found: " << full_path;
                    this->add_error(str.str());
                }

                value = ff::value::create<std::string>(ff::filesystem::to_string(full_path));
            }
        }

        return value;
    }
};

class expand_values_and_templates_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_dict(const ff::dict& dict) override
    {
        ff::dict input_dict = dict;
        std::string template_name;
        bool values_pushed = false;

        ff::value_ptr values_value = input_dict.get(ff::internal::RES_VALUES);
        if (values_value)
        {
            input_dict.set(ff::internal::RES_VALUES, nullptr);

            ff::value_ptr values_dict = values_value->convert_or_default<ff::dict>();
            this->context().push_values().set(values_dict->get<ff::dict>(), false);
            values_pushed = true;
        }

        ff::scope_exit pop_values([values_pushed, this]()
            {
                if (values_pushed)
                {
                    this->context().pop_values();
                }
            });

        ff::value_ptr template_name_value = input_dict.get(ff::internal::RES_TEMPLATE);
        if (template_name_value)
        {
            input_dict.set(ff::internal::RES_TEMPLATE, nullptr);

            if (template_name_value->is_type<std::string>())
            {
                template_name = template_name_value->get<std::string>();
            }
        }

        ff::value_ptr import_value = input_dict.get(ff::internal::RES_IMPORT);
        if (import_value)
        {
            input_dict.set(ff::internal::RES_IMPORT, nullptr);

            if (import_value->is_type<std::string>())
            {
                ff::dict import_dict;
                if (!this->import_file(std::filesystem::path(import_value->get<std::string>()), import_dict))
                {
                    return nullptr;
                }

                import_dict.set(input_dict, false);
                input_dict = import_dict;
            }
        }

        if (!template_name.empty())
        {
            ff::value_ptr template_value = this->context().values().get(template_name);
            if (!template_value)
            {
                std::ostringstream str;
                str << "Invalid template name: " << template_name;
                this->add_error(str.str());
                return nullptr;
            }

            ff::value_ptr dict_value = ff::type::try_get_dict_from_data(template_value);
            if (dict_value)
            {
                ff::dict output_dict = dict_value->get<ff::dict>();
                output_dict.set(input_dict, false);
                return this->transform_dict(output_dict);
            }

            if (!input_dict.empty())
            {
                std::ostringstream str;
                str << "Unexpected extra values for template: " << template_name;
                this->add_error(str.str());
                return nullptr;
            }

            return this->transform_value(template_value);
        }

        return transformer_base::transform_dict(input_dict);
    }

    virtual ff::value_ptr transform_value(ff::value_ptr value) override
    {
        value = transformer_base::transform_value(value);

        if (value && value->is_type<std::string>())
        {
            std::string string_value = value->get<std::string>();

            if (ff::string::starts_with(string_value, ff::internal::RES_PREFIX))
            {
                value = this->context().values().get(string_value.substr(ff::internal::RES_PREFIX.size()));
                value = this->transform_value(value);

                if (!value)
                {
                    std::ostringstream str;
                    str << "Undefined value: " << string_value;
                    this->add_error(str.str());
                }
            }
            else if (ff::string::starts_with(string_value, ff::internal::REF_PREFIX))
            {
                std::string res_name = string_value.substr(ff::internal::REF_PREFIX.size());
                std::shared_ptr<ff::resource> res_val = this->context().set_reference(res_name);
                value = ff::value::create<ff::resource>(res_val);
            }
        }

        return value;
    }

    virtual void async_thread_started(DWORD main_thread_id) override
    {
        transformer_base::async_thread_started(main_thread_id);
        this->context().push_values_from_owner_thread(main_thread_id);
    }

    virtual void async_thread_done() override
    {
        this->context().pop_values();
        transformer_base::async_thread_done();
    }

private:
    bool import_file(const std::filesystem::path& path, ff::dict& json)
    {
        json.clear();

        std::string json_text;
        if (!ff::filesystem::read_text_file(path, json_text))
        {
            std::ostringstream str;
            str << "Failed to read JSON import file: " << path;
            this->add_error(str.str());
            return false;
        }

        ff::dict dict;
        const char* error_pos;
        if (!ff::json_parse(json_text, dict, &error_pos))
        {
            size_t offset = error_pos - json_text.data();
            std::ostringstream str;
            str << "Failed to read JSON import file: " << path << "\r\n" <<
                "Failed parsing JSON at pos: " << offset << "\r\n" <<
                "  -->" << json_text.substr(offset, std::min<size_t>(32, json_text.size() - offset));
            this->add_error(str.str());
            return false;
        }

        ff::value_ptr output_value = this->transform_dict(dict);
        ff::value_ptr output_dict = ff::type::try_get_dict_from_data(output_value);
        if (!output_dict)
        {
            std::ostringstream str;
            str << "Imported JSON is not a dict: " << path;
            this->add_error(str.str());
            return false;
        }

        json = output_dict->get<ff::dict>();
        return true;
    }
};

class start_load_objects_from_dict_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_dict(const ff::dict& dict) override
    {
        ff::value_ptr output_value = transformer_base::transform_dict(dict);
        if (this->is_root())
        {
            // Save references to all root objects

            ff::value_ptr dict_value = ff::type::try_get_dict_from_data(output_value);
            if (dict_value)
            {
                ff::dict output_dict = dict_value->get<ff::dict>();

                for (std::string_view name : output_dict.child_names())
                {
                    ff::value_ptr object_value = output_dict.get(name);
                    if (object_value->is_type<ff::resource_object_base>())
                    {
                        this->context().set_reference(name, object_value);
                    }
                }

                output_value = ff::value::create<ff::dict>(std::move(output_dict));
            }
        }
        else
        {
            // Convert all typed object dicts into objects

            ff::value_ptr output_dict_value = ff::type::try_get_dict_from_data(output_value);
            if (output_dict_value)
            {
                const ff::dict& output_dict = output_dict_value->get<ff::dict>();
                ff::value_ptr type_value = output_dict.get(ff::internal::RES_TYPE);
                ff::value_ptr symbol_value = output_dict.get(ff::internal::RES_SYMBOL);

                if (this->path_depth() == 1)
                {
                    std::string name = this->path();
                    std::string id = (symbol_value && symbol_value->is_type<std::string>())
                        ? symbol_value->get<std::string>()
                        : ::make_symbol_from_name(name);

                    if (id.size())
                    {
                        this->context().set_id_to_name(id, name);
                    }
                }

                if (type_value && type_value->is_type<std::string>())
                {
                    const std::string& type_name = type_value->get<std::string>();
                    const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(type_name);
                    std::shared_ptr<ff::resource_object_base> obj = factory ? factory->load_from_source(output_dict, this->context()) : nullptr;

                    if (obj)
                    {
                        output_value = ff::value::create<ff::resource_object_base>(obj);
                    }
                    else
                    {
                        std::ostringstream str;
                        str << "Failed to create object of type: " << type_name;
                        this->add_error(str.str());
                    }
                }
            }
        }

        return output_value;
    }
};

class finish_load_objects_from_dict_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_value(ff::value_ptr value) override
    {
        value = transformer_base::transform_value(value);

        if (value && value->is_type<ff::resource_object_base>())
        {
            if (!this->finish_loading_object(value->get<ff::resource_object_base>().get()))
            {
                this->add_error("Failed to finish loading resource");
            }
        }

        return value;
    }

private:
    bool finish_loading_object(ff::resource_object_base* obj)
    {
        // See if it's already loading
        {
            std::unique_lock lock(this->mutex);
            if (this->obj_to_finished.find(obj) != this->obj_to_finished.cend())
            {
                return true;
            }

            auto iter = this->obj_to_currently_finishing.find(obj);
            if (iter != this->obj_to_currently_finishing.cend())
            {
                const finishing_info& info = iter->second;
                if (info.thread_id == ::GetCurrentThreadId())
                {
                    this->add_error("Can't finish loading a resource that depends on itself");
                }

                ff::win_event wait_handle = info.event_handle;
                lock.unlock();

                wait_handle.wait();
                return true;
            }
            else
            {
                finishing_info info;
                this->obj_to_currently_finishing.try_emplace(obj, std::move(info));
            }
        }

        // Load dependencies
        bool result = true;
        {
            for (std::shared_ptr<ff::resource> dep : obj->resource_get_dependencies())
            {
                std::shared_ptr<ff::resource_object_base> dep_obj = dep ? dep->value()->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>() : nullptr;
                if (dep_obj && !this->finish_loading_object(dep_obj.get()))
                {
                    std::ostringstream str;
                    str << "Failed to finish loading dependent resource: " << dep->name();
                    this->add_error(str.str());
                    result = false;
                    break;
                }
            }

            result = obj->resource_load_complete(true);
        }

        // Done
        {
            std::scoped_lock lock(this->mutex);

            auto iter = this->obj_to_currently_finishing.find(obj);
            ff::win_event event_handle = iter->second.event_handle;
            this->obj_to_currently_finishing.erase(iter);

            event_handle.set();

            this->obj_to_finished.insert(obj);
        }

        return result;
    }

    struct finishing_info
    {
        finishing_info()
            : thread_id(::GetCurrentThreadId())
        {}

        DWORD thread_id;
        ff::win_event event_handle;
    };

    std::recursive_mutex mutex;
    std::unordered_map<ff::resource_object_base*, finishing_info> obj_to_currently_finishing;
    std::unordered_set<ff::resource_object_base*> obj_to_finished;
};

class extract_resource_siblings_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_dict(const ff::dict& dict) override
    {
        ff::value_ptr root_value = transformer_base::transform_dict(dict);
        if (this->is_root())
        {
            ff::value_ptr dict_value = ff::type::try_get_dict_from_data(root_value);
            if (dict_value)
            {
                ff::dict output_dict = dict_value->get<ff::dict>();

                for (std::string_view name : output_dict.child_names())
                {
                    ff::value_ptr object_value = output_dict.get(name);
                    if (object_value->is_type<ff::resource_object_base>())
                    {
                        std::shared_ptr<ff::resource_object_base> res = object_value->get<ff::resource_object_base>();
                        if (res)
                        {
                            std::shared_ptr<ff::resource> res_value = std::make_shared<ff::resource>(name, object_value);
                            ff::dict siblings = res->resource_get_siblings(res_value);
                            output_dict.set(siblings, false);

                            for (auto& pair : siblings)
                            {
                                std::string id = ::make_symbol_from_name(pair.first);
                                this->context().set_id_to_name(id, pair.first);
                                this->context().set_reference(pair.first, pair.second);
                            }
                        }
                    }
                }

                root_value = ff::value::create<ff::dict>(std::move(output_dict));
            }

            this->context().finalize_missing_references();
        }

        return root_value;
    }
};

class save_objects_to_dict_transformer : public transformer_base
{
public:
    using transformer_base::transformer_base;

protected:
    virtual ff::value_ptr transform_value(ff::value_ptr value) override
    {
        value = transformer_base::transform_value(value);

        if (value && value->is_type<ff::resource_object_base>())
        {
            ff::dict dict;
            std::shared_ptr<ff::resource_object_base> res = value->get<ff::resource_object_base>();
            if (!res || !ff::resource_object_base::save_to_cache_typed(*res, dict))
            {
                this->add_error("Failed to save resource");
                return nullptr;
            }

            value = ff::value::create<ff::dict>(std::move(dict));
            if (!value)
            {
                this->add_error("Failed to save resource");
                return nullptr;
            }
        }

        return value;
    }
};

ff::load_resources_result ff::load_resources_from_json(const ff::dict& json_dict, const std::filesystem::path& base_path, bool debug)
{
    ff::dict dict = json_dict;

    ::transformer_context context(base_path, debug);
    ::expand_file_paths_transformer t1(context);
    ::expand_values_and_templates_transformer t2(context);
    ::start_load_objects_from_dict_transformer t3(context);
    ::extract_resource_siblings_transformer t4(context);
    ::finish_load_objects_from_dict_transformer t5(context);
    ::save_objects_to_dict_transformer t6(context);
    std::array<transformer_base*, 6> transformers = { &t1, &t2, &t3, &t4, &t5, &t6 };

    for (transformer_base* transformer : transformers)
    {
        std::vector<std::string> errors;
        ff::value_ptr new_dict_value = ff::type::try_get_dict_from_data(transformer->visit_dict(dict, errors));

        if (!new_dict_value || !errors.empty() || !context.errors().empty())
        {
            ff::load_resources_result result{ nullptr, std::move(errors)};
            std::copy(context.errors().cbegin(), context.errors().cend(), std::back_inserter(result.errors));

            for (const std::string& error : result.errors)
            {
                ff::log::write(ff::log::type::resource_load, "Load resource error: ", error, "\r\n");
            }

            return result;
        }

        dict = new_dict_value->get<ff::dict>();
    }

    // All input files for every single resource
    if (debug)
    {
        std::vector<std::string> path_strings;
        for (const std::filesystem::path& path : context.paths())
        {
            path_strings.push_back(ff::filesystem::to_string(path));
        }

        dict.set<std::vector<std::string>>(ff::internal::RES_FILES, std::move(path_strings));
    }

    // Per-resource input files
    if (debug)
    {
        for (std::string_view name : dict.child_names())
        {
            ff::value_ptr child_value = dict.get(name);
            if (child_value->is_type<ff::dict>())
            {
                std::vector<std::string> child_path_strings;
                for (const std::filesystem::path& path : context.paths(name))
                {
                    child_path_strings.push_back(ff::filesystem::to_string(path));
                }

                if (!child_path_strings.empty())
                {
                    ff::dict child_dict = child_value->get<ff::dict>();
                    child_dict.set<std::vector<std::string>>(ff::internal::RES_FILES, std::move(child_path_strings));
                    dict.set<ff::dict>(name, std::move(child_dict));
                }
            }
        }
    }

    // Output files (usually PDBs from shader compilation)
    if (debug)
    {
        ff::dict output_files_dict;

        for (auto& i : context.output_files())
        {
            output_files_dict.set<ff::data_base>(i.first, i.second, ff::saved_data_type::none);
        }

        dict.set<ff::dict>(ff::internal::RES_OUTPUT_FILES, std::move(output_files_dict));
    }

    // C++ header namespace
    if (!dict.get(ff::internal::RES_NAMESPACE))
    {
        dict.set<std::string>(ff::internal::RES_NAMESPACE, "assets");
    }

    // C++ IDs
    {
        ff::dict id_dict;

        for (auto& i : context.id_to_name())
        {
            id_dict.set<std::string>(i.first, i.second);
        }

        dict.set<ff::dict>(ff::internal::RES_ID_SYMBOLS, std::move(id_dict));
    }

    return ff::load_resources_result{ std::make_shared<ff::resource_objects>(dict) };
}
