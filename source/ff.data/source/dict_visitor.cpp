#include "pch.h"
#include "dict.h"
#include "dict_v.h"
#include "dict_visitor.h"
#include "saved_data.h"
#include "saved_data_v.h"
#include "value.h"
#include "value_vector.h"

ff::dict_visitor_base::dict_visitor_base()
{}

ff::dict_visitor_base::~dict_visitor_base()
{}

ff::value_ptr ff::dict_visitor_base::visit_dict(const ff::dict& dict, std::vector<std::string>& errors)
{
    ff::value_ptr transformed_dict_value = this->transform_dict(dict);
    errors = std::move(this->errors);
    return errors.empty() ? transformed_dict_value : nullptr;
}

void ff::dict_visitor_base::add_error(std::string_view text)
{
    std::ostringstream str;
    str << ::GetCurrentThreadId() << "> " << this->path() << " : " << text;

    std::scoped_lock lock(this->mutex);
    this->errors.push_back(str.str());
}

bool ff::dict_visitor_base::async_allowed(const ff::dict& dict)
{
    return false;
}

std::string ff::dict_visitor_base::path() const
{
    std::scoped_lock lock(this->mutex);
    auto iter = this->thread_to_path.find(::GetCurrentThreadId());
    if (iter == this->thread_to_path.cend())
    {
        return std::string();
    }

    std::ostringstream str;
    bool first = true;

    for (const std::string& part : iter->second)
    {
        if (!first && part.find('[') != 0)
        {
            str << '/';
        }

        str << part;
        first = false;
    }

    return str.str();
}

std::string ff::dict_visitor_base::path_root_name() const
{
    std::string path = this->path();
    size_t slash_pos = path.find('/');
    if (slash_pos != std::string::npos)
    {
        path = path.substr(0, slash_pos);
    }

    return path;
}

size_t ff::dict_visitor_base::path_depth() const
{
    std::scoped_lock lock(this->mutex);
    auto i = this->thread_to_path.find(::GetCurrentThreadId());
    return (i != this->thread_to_path.cend()) ? i->second.size() : 0;
}

void ff::dict_visitor_base::push_path(std::string_view name)
{
    std::scoped_lock lock(this->mutex);
    auto iter = this->thread_to_path.try_emplace(::GetCurrentThreadId(), std::vector<std::string>()).first;
    iter->second.emplace_back(name);
}

void ff::dict_visitor_base::pop_path()
{
    std::scoped_lock lock(this->mutex);
    auto iter = this->thread_to_path.find(::GetCurrentThreadId());
    if (iter != this->thread_to_path.cend())
    {
        std::vector<std::string>& paths = iter->second;
        paths.pop_back();

        if (paths.empty())
        {
            this->thread_to_path.erase(iter);
        }
    }
}

bool ff::dict_visitor_base::is_root() const
{
    return this->path_depth() == 0;
}

ff::value_ptr ff::dict_visitor_base::transform_root_value(ff::value_ptr value)
{
    return this->transform_value(value);
}

void ff::dict_visitor_base::async_thread_started(DWORD main_thread_id)
{
    std::scoped_lock lock(this->mutex);
    auto iter = this->thread_to_path.find(main_thread_id);
    if (iter != this->thread_to_path.cend())
    {
        std::vector<std::string> paths = iter->second;
        this->thread_to_path.emplace(::GetCurrentThreadId(), std::move(paths));
    }
}

void ff::dict_visitor_base::async_thread_done()
{
    std::scoped_lock lock(this->mutex);
    this->thread_to_path.erase(::GetCurrentThreadId());
}

ff::value_ptr ff::dict_visitor_base::transform_dict(const ff::dict& dict)
{
    if (this->async_allowed(dict))
    {
        return this->transform_dict_async(dict);
    }

    bool root = this->is_root();
    ff::dict output_dict = dict;

    for (std::string_view name : dict.child_names())
    {
        this->push_path(name);

        ff::value_ptr old_value = dict.get(name);
        ff::value_ptr new_value = root
            ? this->transform_root_value(old_value)
            : this->transform_value(old_value);
        output_dict.set(name, new_value);

        this->pop_path();
    }

    return ff::value::create<ff::dict>(std::move(output_dict));
}

ff::value_ptr ff::dict_visitor_base::transform_dict_async(const ff::dict& dict)
{
    bool root = this->is_root();
    std::vector<std::string_view> names = dict.child_names();
    std::vector<ff::value_ptr> values(names.size());
    std::vector<ff::win_event> value_events(names.size());
    DWORD main_thread_id = ::GetCurrentThreadId();

    for (size_t i = 0; i < names.size(); i++)
    {
        values[i] = dict.get(names[i]);
        value_events[i] = ff::win_event();

        ff::thread_pool::add_task([this, root, main_thread_id, i, &names, &values, &value_events]()
            {
                this->async_thread_started(main_thread_id);
                this->push_path(names[i]);

                values[i] = root
                    ? this->transform_root_value(values[i])
                    : this->transform_value(values[i]);

                this->pop_path();
                this->async_thread_done();

                value_events[i].set();
            });
    }

    std::vector<HANDLE> handles(value_events.size());
    for (size_t i = 0; i < value_events.size(); i++)
    {
        handles[i] = value_events[i];
    }

    while (!handles.empty())
    {
        size_t handle_count = std::min<size_t>(ff::thread_dispatch::maximum_wait_objects, handles.size());
        if (ff::wait_for_all_handles(handles.data(), handle_count))
        {
            handles.erase(handles.cbegin(), handles.cbegin() + handle_count);
        }
        else
        {
            handles.clear();
        }
    }

    ff::dict output_dict;
    output_dict.reserve(names.size());

    for (size_t i = 0; i < names.size(); i++)
    {
        output_dict.set(names[i], values[i]);
    }

    return ff::value::create<ff::dict>(std::move(output_dict));
}

ff::value_ptr ff::dict_visitor_base::transform_vector(const std::vector<ff::value_ptr>& values)
{
    std::vector<ff::value_ptr> output_values;
    output_values.reserve(values.size());

    for (ff::value_ptr value : values)
    {
        std::ostringstream str;
        str << '[' << output_values.size() << ']';
        this->push_path(str.str());

        ff::value_ptr new_value = this->transform_value(value);
        if (new_value)
        {
            output_values.push_back(new_value);
        }

        this->pop_path();
    }

    return ff::value::create<ff::value_vector>(std::move(output_values));
}

ff::value_ptr ff::dict_visitor_base::transform_value(ff::value_ptr value)
{
    ff::value_ptr output_value = value;
    ff::value_ptr dict_val = ff::type::try_get_dict_from_data(value);

    if (dict_val)
    {
        output_value = this->transform_dict(dict_val->get<ff::dict>());
    }
    else if (value->is_type<ff::value_vector>())
    {
        output_value = this->transform_vector(value->get<ff::value_vector>());
    }

    return output_value;
}
