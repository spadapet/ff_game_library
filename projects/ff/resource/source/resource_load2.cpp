#include "pch.h"
#include "resource.h"
#include "resource_v.h"
#include "resource_load.h"
#include "resource_load_context.h"
#include "resource_object_base.h"
#include "resource_object_factory_base.h"
#include "resource_object_v.h"

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
            this->errors_.emplace_back(text);
        }
    }

    virtual bool debug() const override
    {
        return this->debug_;
    }

    const ff::dict& values() const
    {
        static ff::dict empty_dict;
        std::lock_guard lock(this->mutex);
        auto iter = this->thread_to_values.find(::GetCurrentThreadId());
        return iter != this->thread_to_values.cend() ? iter->second.back() : empty_dict;
    }

    ff::dict& push_values()
    {
        std::lock_guard lock(this->mutex);

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
        std::lock_guard lock(this->mutex);

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
        std::lock_guard lock(this->mutex);

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

    std::shared_ptr<ff::resource> set_reference(std::shared_ptr<ff::resource> res)
    {
        std::string name(res->name());
        std::lock_guard lock(this->mutex);

        auto i = this->name_to_resource.find(name);
        if (i != this->name_to_resource.cend())
        {
            if (res->value()->is_type<nullptr_t>())
            {
                res = i->second;
            }
            else
            {
                i->second->new_resource(res);
            }
        }
        else
        {
            this->name_to_resource.try_emplace(std::move(name), res);
        }

        return res;
    }

    void add_file(const std::filesystem::path& path)
    {
        std::filesystem::path path_canon = std::filesystem::canonical(path);

        std::lock_guard lock(this->mutex);
        this->paths_.insert(std::move(path_canon));
    }

    std::vector<std::filesystem::path> paths() const
    {
        std::lock_guard lock(this->mutex);

        std::vector<std::filesystem::path> paths;
        paths.reserve(this->paths_.size());

        for (const std::filesystem::path& path : this->paths_)
        {
            paths.push_back(path);
        }

        return paths;
    }

private:
    mutable std::recursive_mutex mutex;
    std::filesystem::path base_path_;
    std::vector<std::string> errors_;
    std::unordered_map<DWORD, std::vector<ff::dict>> thread_to_values;
    std::unordered_map<std::string, std::shared_ptr<ff::resource>> name_to_resource;
    std::unordered_set<std::filesystem::path, ff::hash<std::filesystem::path>> paths_;
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
                std::filesystem::path full_path = std::filesystem::canonical(this->context().base_path() / path_value);

                std::error_code ec;
                if (std::filesystem::exists(full_path, ec))
                {
                    this->context().add_file(full_path);
                }
                else
                {
                    std::ostringstream str;
                    str << "File not found: " << full_path;
                    this->add_error(str.str());
                }

                value = ff::value::create<std::string>(full_path.string());
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

        ff::at_scope pop_values([values_pushed, this]()
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

            ff::value_ptr dict_value = template_value->try_convert<ff::dict>();
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
                std::shared_ptr<ff::resource> res_val = std::make_shared<ff::resource>(
                    string_value.substr(ff::internal::REF_PREFIX.size()),
                    ff::value::create<nullptr_t>());
                res_val = this->context().set_reference(res_val);

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
            str << "Failed to read JSON import file: " << path << std::endl <<
                "Failed parsing JSON at pos: " << offset << std::endl <<
                "  -->" << json_text.substr(offset, std::min<size_t>(32, json_text.size() - offset));
            this->add_error(str.str());
            return false;
        }

        ff::value_ptr output_value = this->transform_dict(dict);
        ff::value_ptr output_dict = output_value->try_convert<ff::dict>();
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

            ff::value_ptr dict_value = output_value->try_convert<ff::dict>();
            if (dict_value)
            {
                ff::dict output_dict = dict_value->get<ff::dict>();

                for (std::string_view name : output_dict.child_names())
                {
                    ff::value_ptr object_value = output_dict.get(name);
                    if (object_value->is_type<ff::resource_object_base>())
                    {
                        this->context().set_reference(std::make_shared<ff::resource>(name, object_value));
                    }
                }

                output_value = ff::value::create<ff::dict>(std::move(output_dict));
            }
        }
        else
        {
            // Convert all typed object dicts into objects

            ff::value_ptr output_dict_value = output_value->try_convert<ff::dict>();
            if (output_dict_value)
            {
                const ff::dict& output_dict = output_dict_value->get<ff::dict>();
                ff::value_ptr type_value = output_dict.get(ff::internal::RES_TYPE);

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
            auto lock = std::make_unique<std::lock_guard<std::recursive_mutex>>(this->mutex);
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

                ff::win_handle wait_handle = info.event_handle.duplicate();
                lock.reset();

                ff::wait_for_handle(wait_handle);
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
                std::shared_ptr<ff::resource_object_base> dep_obj = dep->value()->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>();
                if (dep_obj && !this->finish_loading_object(dep_obj.get()))
                {
                    std::ostringstream str;
                    str << "Failed to finish loading dependent resource: " << dep->name();
                    this->add_error(str.str());
                    result = false;
                    break;
                }
            }

            result = obj->resource_load_from_source_complete();
        }

        // Done
        {
            std::lock_guard lock(this->mutex);

            auto iter = this->obj_to_currently_finishing.find(obj);
            ff::win_handle event_handle = std::move(iter->second.event_handle);
            this->obj_to_currently_finishing.erase(iter);

            ::SetEvent(event_handle);

            this->obj_to_finished.insert(obj);
        }

        return result;
    }

    struct finishing_info
    {
        finishing_info()
            : thread_id(::GetCurrentThreadId())
            , event_handle(ff::create_event())
        {}

        DWORD thread_id;
        ff::win_handle event_handle;
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
            ff::value_ptr dict_value = root_value->try_convert<ff::dict>();
            if (dict_value)
            {
                ff::dict output_dict = dict_value->get<ff::dict>();

                for (std::string_view name : output_dict.child_names())
                {
                    ff::value_ptr object_value = output_dict.get(name);
                    if (object_value->is_type<ff::resource_object_base>())
                    {
                        auto res = object_value->get<ff::resource_object_base>();
                        if (res)
                        {
                            std::shared_ptr<ff::resource> res_value = std::make_shared<ff::resource>(name, object_value);
                            output_dict.set(res->resource_get_siblings(res_value), false);
                        }
                    }
                }

                root_value = ff::value::create<ff::dict>(std::move(output_dict));
            }
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
            bool allow_compress = true;
            auto res = value->get<ff::resource_object_base>();
            if (!res || !ff::resource_object_base::save_to_cache_typed(*res, dict, allow_compress))
            {
                this->add_error("Failed to save resource");
                return nullptr;
            }

            value = ff::value::create<ff::dict>(std::move(dict), allow_compress && this->path_depth() == 1);
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
    ::finish_load_objects_from_dict_transformer t4(context);
    ::extract_resource_siblings_transformer t5(context);
    ::save_objects_to_dict_transformer t6(context);
    std::array<transformer_base*, 6> transformers = { &t1, &t2, &t3, &t4, &t5, &t6 };

    for (transformer_base* transformer : transformers)
    {
        std::vector<std::string> errors;
        ff::value_ptr new_dict_value = transformer->visit_dict(dict, errors)->try_convert<ff::dict>();

        if (!new_dict_value || !errors.empty())
        {
            ff::load_resources_result result{};
            result.status = false;
            result.errors = std::move(errors);
            return result;
        }

        dict = new_dict_value->get<ff::dict>();
    }

    std::vector<std::filesystem::path> paths = context.paths();
    if (!paths.empty())
    {
        std::vector<std::string> path_strings;
        path_strings.reserve(paths.size());

        for (const std::filesystem::path& path : paths)
        {
            path_strings.push_back(path.string());
        }

        dict.set<std::vector<std::string>>(ff::internal::RES_FILES, std::move(path_strings));
    }

    ff::load_resources_result result{};
    result.status = true;
    result.dict = std::move(dict);
    return result;
}
