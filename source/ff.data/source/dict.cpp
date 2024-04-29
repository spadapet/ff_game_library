#include "pch.h"
#include "data.h"
#include "data_v.h"
#include "dict.h"
#include "dict_v.h"
#include "persist.h"
#include "saved_data.h"
#include "stream.h"
#include "value.h"
#include "value_vector.h"

using namespace std::string_view_literals;

static const size_t DICT_PERSIST_COOKIE = ff::stable_hash_func("ff::dict"sv);

static const std::string& get_cached_string(std::string_view str)
{
    static std::forward_list<std::string> string_list;
    static std::unordered_map<std::string_view, const std::string&> string_map;
    static std::shared_mutex mutex;

    // Check if it's cached already
    {
        std::shared_lock lock(mutex);
        auto i = string_map.find(str);
        if (i != string_map.cend())
        {
            return i->second;
        }
    }

    // Add to the cache
    {
        std::unique_lock lock(mutex);
        const std::string& new_str = string_list.emplace_front(str);
        return string_map.try_emplace(new_str, new_str).first->second;
    }
}

bool ff::dict::operator==(const dict& other) const
{
    if (this->size() == other.size())
    {
        for (const auto& i : *this)
        {
            value_ptr other_val = other.get(i.first);
            if (!i.second->equals(other_val))
            {
                return false;
            }
        }

        return true;
    }

    return false;
}

std::ostream& ff::dict::operator<<(std::ostream& ostream) const
{
    this->print(ostream);
    return ostream;
}

bool ff::dict::empty() const
{
    return this->map.empty();
}

size_t ff::dict::size() const
{
    return this->map.size();
}

void ff::dict::reserve(size_t count)
{
    this->map.reserve(count);
}

void ff::dict::clear()
{
    this->map.clear();
}

void ff::dict::set(const dict& other, bool merge_child_dicts)
{
    for (const auto& i : other)
    {
        value_ptr new_val = i.second;
        value_ptr new_dict = merge_child_dicts ? ff::type::try_get_dict_from_data(new_val) : nullptr;
        if (new_dict)
        {
            value_ptr old_dict = ff::type::try_get_dict_from_data(this->get(i.first));
            if (old_dict)
            {
                dict combined_dict = old_dict->get<dict>();
                combined_dict.set(new_dict->get<dict>(), merge_child_dicts);
                new_val = value::create<dict>(std::move(combined_dict));
            }
        }

        this->set(i.first, new_val);
    }
}

void ff::dict::set(std::string_view name, const value* value)
{
    if (!value)
    {
        this->map.erase(name);
    }
    else
    {
        this->map.insert_or_assign(::get_cached_string(name), value);
    }
}

void ff::dict::set_bytes(std::string_view name, const void* data, size_t size)
{
    std::vector<uint8_t> data_vector;
    if (size)
    {
        data_vector.resize(size);
        std::memcpy(data_vector.data(), data, size);
    }

    std::shared_ptr<ff::data_base> value = std::make_shared<ff::data_vector>(std::move(data_vector));
    this->set<ff::data_base>(name, value, ff::saved_data_type::none);
}

ff::value_ptr ff::dict::get(std::string_view name) const
{
    value_ptr value = this->get_by_path(name);
    if (!value)
    {
        auto i = this->map.find(name);
        value = (i != this->map.cend()) ? i->second : nullptr;
    }

    return value;
}

bool ff::dict::get_bytes(std::string_view name, void* data, size_t size) const
{
    std::shared_ptr<ff::data_base> value = this->get<ff::data_base>(name);
    if (value && value->size() == size)
    {
        std::memcpy(data, value->data(), size);
        return true;
    }

    return false;
}

std::vector<std::string_view> ff::dict::child_names(bool sorted) const
{
    std::vector<std::string_view> names;
    names.reserve(this->size());

    for (const auto& i : *this)
    {
        names.push_back(i.first);
    }

    if (sorted)
    {
        std::sort(names.begin(), names.end());
    }

    return names;
}

bool ff::dict::load_child_dicts()
{
    bool changed = false;

    for (auto& i : *this)
    {
        value_ptr val = i.second;
        if (val->is_type<ff::value_vector>())
        {
            bool changed_vector = false;
            ff::value_vector values = val->get<ff::value_vector>();
            for (size_t h = 0; h < values.size(); h++)
            {
                value_ptr dict_val = ff::type::try_get_dict_from_data(values[h]);
                if (dict_val)
                {
                    dict new_dict = dict_val->get<dict>();
                    new_dict.load_child_dicts();
                    values[h] = value::create<dict>(std::move(new_dict));
                    changed_vector = true;
                }
            }

            if (changed_vector)
            {
                i.second = value::create<ff::value_vector>(std::move(values));
                changed = true;
            }
        }
        else
        {
            value_ptr dict_val = ff::type::try_get_dict_from_data(val);
            if (dict_val)
            {
                dict new_dict = dict_val->get<dict>();
                new_dict.load_child_dicts();

                i.second = value::create<dict>(std::move(new_dict));
                changed = true;
            }
        }
    }

    return changed;
}

bool ff::dict::save(ff::writer_base& writer, ff::push_base<ff::dict::location_t>* saved_locations) const
{
    const size_t start_pos = writer.pos();
    const size_t size = this->size();
    assert_ret_val(ff::save(writer, ::DICT_PERSIST_COOKIE) && ff::save(writer, size), false);

    for (const auto& i : *this)
    {
        ff::dict::location_t location{ i.first };

        assert_ret_val(ff::save(writer, i.first), false);
        location.offset = writer.pos() - start_pos;

        assert_ret_val(i.second->save_typed(writer), false);
        location.size = writer.pos() - location.offset;

        if (saved_locations)
        {
            saved_locations->push(location);
        }
    }

    return true;
}

bool ff::dict::load(ff::reader_base& reader, ff::dict& data)
{
    size_t cookie, size;
    if (!ff::load(reader, cookie) || cookie != ::DICT_PERSIST_COOKIE ||
        !ff::load(reader, size))
    {
        return false;
    }

    data.reserve(data.size() + size);

    std::string name;
    for (size_t i = 0; i < size; i++)
    {
        if (!ff::load(reader, name))
        {
            return false;
        }

        ff::value_ptr val = ff::value::load_typed(reader);
        if (!val)
        {
            return false;
        }

        data.set(name, val);
    }

    return true;
}

ff::dict::iterator ff::dict::begin()
{
    return this->map.begin();
}

ff::dict::const_iterator ff::dict::begin() const
{
    return this->map.begin();
}

ff::dict::const_iterator ff::dict::cbegin() const
{
    return this->map.cbegin();
}

ff::dict::iterator ff::dict::end()
{
    return this->map.end();
}

ff::dict::const_iterator ff::dict::end() const
{
    return this->map.end();
}

ff::dict::const_iterator ff::dict::cend() const
{
    return this->map.cend();
}

void ff::dict::print(std::ostream& output) const
{
    ff::value::create<dict>(dict(*this))->print_tree(output);
}

void ff::dict::debug_print() const
{
    ff::log::write(ff::log::type::debug, *this);
}

ff::value_ptr ff::dict::get_by_path(std::string_view path) const
{
    if (path.size() && path[0] == '/')
    {
        size_t next_thing = path.find_first_of("/[", 1, 2);
        std::string_view name = path.substr(1, next_thing - ((next_thing != std::string_view::npos) ? 1 : 0));
        value_ptr value = this->get(name);

        for (; value && next_thing != std::string_view::npos; next_thing = path.find_first_of("/[", next_thing + 1, 2))
        {
            if (path[next_thing] == '/')
            {
                value = value->named_child(path.substr(next_thing));
                break;
            }
            else // '['
            {
                size_t index;
                std::from_chars_result result = std::from_chars(path.data() + next_thing + 1, path.data() + path.size(), index);
                value = (result.ec != std::errc::invalid_argument && result.ptr < path.data() + path.size() && *result.ptr == ']')
                    ? value->index_child(index)
                    : nullptr;
            }
        }

        return value;
    }

    return nullptr;
}

std::ostream& std::operator<<(std::ostream& ostream, const ff::dict& value)
{
    return value.operator<<(ostream);
}
