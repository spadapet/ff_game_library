#include "pch.h"
#include "dict.h"
#include "persist.h"
#include "stream.h"
#include "value/value.h"
// TODO: Include type/dict_value.h

static const std::string& get_cached_string(std::string_view str)
{
    static std::unordered_map<std::string_view, std::string> map;
    static std::shared_mutex mutex;

    // Check if it's cached already
    {
        std::shared_lock lock(mutex);
        auto i = map.find(str);
        if (i != map.cend())
        {
            return i->second;
        }
    }

    // Add to the cache
    {
        std::string new_str(str);
        std::unique_lock lock(mutex);
        return map.try_emplace(new_str, std::move(new_str)).first->second;
    }
}

bool ff::dict::operator==(const dict& other) const
{
    if (this->size() == other.size())
    {
        std::vector<std::string_view> names1 = this->child_names();
        std::vector<std::string_view> names2 = other.child_names();

        std::sort(names1.begin(), names1.end());
        std::sort(names2.begin(), names2.end());

        if (names1 == names2)
        {
            for (const std::string_view& name : names1)
            {
                if (!this->get(name)->equals(other.get(name)))
                {
                    return false;
                }
            }

            return true;
        }
    }

    return false;
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

void ff::dict::set(const dict& other, bool merge_child_dicts)
{
    // TODO: after saved dicts exist
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

std::vector<std::string_view> ff::dict::child_names() const
{
    std::vector<std::string_view> names;
    names.reserve(this->size());

    for (const auto& i : *this)
    {
        names.push_back(i.first);
    }

    return names;
}

void ff::dict::load_child_dicts()
{
    // TODO: after saved dicts exist
}

bool ff::dict::save(writer_base& writer)
{
    size_t size = this->size();
    if (!ff::save(writer, size))
    {
        return false;
    }

    for (const auto& i : *this)
    {
        if (!ff::save(writer, i.first) || !i.second->save_typed(writer))
        {
            return false;
        }
    }

    return true;
}

bool ff::dict::load(reader_base& reader, dict& data)
{
    size_t size;
    if (!ff::load(reader, size))
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
    // TODO:
    // ff::value::create<dict>(dict(*this))->print(output);
}

void ff::dict::debug_print() const
{
#ifdef _DEBUG
    std::stringstream output;
    this->print(output);
    ::OutputDebugString(ff::string::to_wstring(output.str()).c_str());
#endif
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
