#include "pch.h"
#include "dict_v.h"
#include "string_v.h"
#include "value.h"

ff::value_type::value_type(std::string_view name)
    : name(name)
{
    std::ostringstream hash_name;
    hash_name << name << "," << name.size();
    this->lookup_id = static_cast<uint32_t>(ff::hash<std::string>()(hash_name.str()));
}

ff::value_type::~value_type()
{}

std::string_view ff::value_type::type_name() const
{
    return this->name;
}

uint32_t ff::value_type::type_lookup_id() const
{
    return this->lookup_id;
}

bool ff::value_type::equals(const value* val1, const value* val2) const
{
    return false;
}

const void* ff::value_type::try_cast(const value* val, std::type_index type) const
{
    return nullptr;
}

ff::value_ptr ff::value_type::try_convert_to(const value* val, std::type_index type) const
{
    return nullptr;
}

ff::value_ptr ff::value_type::try_convert_from(const value* other) const
{
    return nullptr;
}

bool ff::value_type::can_have_named_children() const
{
    return false;
}

ff::value_ptr ff::value_type::named_child(const value* val, std::string_view name) const
{
    return nullptr;
}

std::vector<std::string_view> ff::value_type::child_names(const value* val) const
{
    return std::vector<std::string_view>();
}

bool ff::value_type::can_have_indexed_children() const
{
    return false;
}

ff::value_ptr ff::value_type::index_child(const value* val, size_t index) const
{
    return nullptr;
}

size_t ff::value_type::index_child_count(const value* val) const
{
    return 0;
}

ff::value_ptr ff::value_type::load(reader_base& reader) const
{
    return nullptr;
}

bool ff::value_type::save(const value* val, writer_base& writer) const
{
    return false;
}

static void write_sanitized_string(std::string_view str, std::ostream& output)
{
    std::string str_copy;
    if (str.find_first_of("\r\n") != std::string::npos)
    {
        str_copy.reserve(str.size());

        for (char ch : str)
        {
            if (ch == '\r')
            {
                str_copy += "\\r";
            }
            else if (ch == '\n')
            {
                str_copy += "\\n";
            }
            else
            {
                str_copy += ch;
            }
        }

        str = str_copy;
    }

    if (str.size() > 80)
    {
        output << str.substr(0, 40) << "..." << str.substr(str.size() - 40, 40);
    }
    else
    {
        output << str;
    }
}

static void print_tree(std::string_view name, ff::value_ptr val, size_t level, std::ostream& output)
{
    std::string spaces(level * 4, ' ');

    if (name.size())
    {
        output << spaces << name << ": ";
    }

    // Write value
    {
        std::ostringstream val_output;
        val->print(val_output);
        ::write_sanitized_string(val_output.str(), output);
        output << std::endl;
    }

    std::vector<std::string_view> child_names;

    if (val->can_have_named_children())
    {
        child_names = val->child_names();
    }
    else if (val->can_have_indexed_children())
    {
        for (size_t i = 0; i < val->index_child_count(); i++)
        {
            output << spaces << "    [" << i << "] ";
            ::print_tree(std::string_view(""), val->index_child(i), level + 2, output);
        }
    }
    else
    {
        ff::value_ptr dict_val = val->try_convert<ff::dict>();
        if (dict_val)
        {
            child_names = dict_val->child_names();
        }
    }

    std::sort(child_names.begin(), child_names.end());
    for (const std::string_view& child_name : child_names)
    {
        ::print_tree(child_name, val->named_child(child_name), level + 1, output);
    }
}

void ff::value_type::print(const value* val, std::ostream& output) const
{
    value_ptr string_val = val->try_convert<std::string>();
    if (string_val)
    {
        output << string_val->get<std::string>();
    }
    else if (val->can_have_indexed_children())
    {
        output << "<" << this->type_name() << "[" << val->index_child_count() << "]>";
    }
    else if (val->can_have_named_children())
    {
        output << "<" << this->type_name() << "[" << val->child_names().size() << "]>";
    }
    else
    {
        output << "<" << this->type_name() << ">";
    }
}

void ff::value_type::print_tree(const value* val, std::ostream& output) const
{
    ::print_tree("", val, 0, output);
}
