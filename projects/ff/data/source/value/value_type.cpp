#include "pch.h"
#include "value.h"

static uint32_t next_lookup_id()
{
    static std::atomic_uint32_t next = 0;
    return next.fetch_add(1);
}

ff::data::value_type::value_type()
    : lookup_id(::next_lookup_id())
{}

ff::data::value_type::~value_type()
{}

uint32_t ff::data::value_type::type_lookup_id() const
{
    return this->lookup_id;
}

bool ff::data::value_type::equals(const value* val1, const value* val2) const
{
    return false;
}

ff::data::value_ptr ff::data::value_type::try_convert_to(const value* val, std::type_index type) const
{
    return nullptr;
}

ff::data::value_ptr ff::data::value_type::try_convert_from(const value* other) const
{
    return nullptr;
}

bool ff::data::value_type::can_have_named_children() const
{
    return false;
}

ff::data::value_ptr ff::data::value_type::named_child(const value* val, std::string_view name) const
{
    return nullptr;
}

std::vector<std::string> ff::data::value_type::child_names(const value* val) const
{
    return std::vector<std::string>();
}

bool ff::data::value_type::can_have_indexed_children() const
{
    return false;
}

ff::data::value_ptr ff::data::value_type::index_child(const value* val, size_t index) const
{
    return nullptr;
}

size_t ff::data::value_type::index_child_count(const value* val) const
{
    return 0;
}

ff::data::value_ptr ff::data::value_type::load(reader_base& reader) const
{
    return nullptr;
}

bool ff::data::value_type::save(const value* val, writer_base& writer) const
{
    return false;
}

void ff::data::value_type::print(std::ostream& output) const
{
    //ff::ValuePtr stringValue = value->Convert<ff::StringValue>();
    //if (stringValue)
    //{
    //    return stringValue->GetValue<ff::StringValue>();
    //}
    //
    //if (value->CanHaveIndexedChildren())
    //{
    //    return ff::String::format_new(L"<%s[%lu]>", value->GetTypeName().c_str(), value->GetIndexChildCount());
    //}
    //
    //if (value->CanHaveNamedChildren())
    //{
    //    return ff::String::format_new(L"<%s[%lu]>", value->GetTypeName().c_str(), value->GetChildNames().Size());
    //}
    //
    //return ff::String::format_new(L"<%s>", value->GetTypeName().c_str());
}

void ff::data::value_type::print_tree(std::ostream& output) const
{}
