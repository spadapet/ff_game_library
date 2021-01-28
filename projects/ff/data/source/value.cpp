#include "pch.h"
#include "persist.h"
#include "value.h"

static std::vector<std::unique_ptr<ff::value_type>> all_value_types;
static std::unordered_map<std::type_index, ff::value_type*> type_index_to_value_type;
static std::unordered_map<uint32_t, ff::value_type*> id_to_value_type;

ff::value::value()
    : type_(nullptr)
    , refs(0)
{}

ff::value::~value()
{
    assert(this->refs.load() == 0 || this->refs.load() == 1);
}

bool ff::value::can_have_named_children() const
{
    return this->type()->can_have_named_children();
}

ff::value_ptr ff::value::named_child(std::string_view name) const
{
    return this->type()->named_child(this, name);
}

std::vector<std::string_view> ff::value::child_names() const
{
    return this->type()->child_names(this);
}

bool ff::value::can_have_indexed_children() const
{
    return this->type()->can_have_indexed_children();
}

ff::value_ptr ff::value::index_child(size_t index) const
{
    return this->type()->index_child(this, index);
}

size_t ff::value::index_child_count() const
{
    return this->type()->index_child_count(this);
}

ff::value_ptr ff::value::load_typed(reader_base& reader)
{
    uint32_t persist_id;
    if (ff::load(reader, persist_id))
    {
        const value_type* type = ff::value::get_type_by_lookup_id(persist_id);
        if (type)
        {
            return type->load(reader);
        }
    }

    assert(false);
    return nullptr;
}

bool ff::value::save_typed(writer_base& writer) const
{
    const value_type* type = this->type();
    uint32_t persist_id = type->type_lookup_id();
    bool status = ff::save(writer, persist_id) && type->save(this, writer);
    assert(status);
    return status;
}

void ff::value::print(std::ostream& output) const
{
    this->type()->print(this, output);
}

void ff::value::print_tree(std::ostream& output) const
{
    this->type()->print_tree(this, output);
}

void ff::value::debug_print_tree() const
{
#ifdef _DEBUG
    std::ostringstream output;
    this->print_tree(output);
    ff::log::write_debug(output);
#endif
}

const ff::value_type* ff::value::type() const
{
    return this->type_;
}

bool ff::value::equals(const value* other) const
{
    if (!this)
    {
        return !other;
    }

    if (this == other)
    {
        return true;
    }

    if (!other || this->type()->type_index() != other->type()->type_index())
    {
        return false;
    }

    return this->type()->equals(this, other);
}

void ff::value::add_ref() const
{
    if (this->refs.load() != 0)
    {
        this->refs.fetch_add(1);
    }
}

void ff::value::release_ref() const
{
    if (this->refs.load() != 0 && this->refs.fetch_sub(1) == 2)
    {
        const value_type* type = this->type();
        value* self = const_cast<value*>(this);
        type->destruct(self);
        ff::internal::value_allocator::delete_bytes(self, type->size_of());
    }
}

bool ff::value::register_type(std::unique_ptr<value_type>&& type)
{
    if (::id_to_value_type.find(type->type_lookup_id()) == ::id_to_value_type.cend() &&
        ::type_index_to_value_type.find(type->type_index()) == ::type_index_to_value_type.cend())
    {
        ::type_index_to_value_type.try_emplace(type->type_index(), type.get());
        ::id_to_value_type.try_emplace(type->type_lookup_id(), type.get());
        ::all_value_types.push_back(std::move(type));
        return true;
    }

    assert(false);
    return false;
}

const ff::value_type* ff::value::get_type(std::type_index type_index)
{
    auto i = ::type_index_to_value_type.find(type_index);
    if (i != ::type_index_to_value_type.cend())
    {
        return i->second;
    }

    assert(false);
    return nullptr;
}

const ff::value_type* ff::value::get_type_by_lookup_id(uint32_t id)
{
    auto i = ::id_to_value_type.find(id);
    if (i != ::id_to_value_type.cend())
    {
        return i->second;
    }

    assert(false);
    return nullptr;
}

bool ff::value::is_type(std::type_index type_index) const
{
    return this && (type_index == typeid(ff::value) || this->type()->type_index() == type_index);
}

bool ff::value::is_same_type(const value* other) const
{
    return this && other && this->type() == other->type();
}

const void* ff::value::try_cast(std::type_index type_index) const
{
    return this ? this->type()->try_cast(this, type_index) : nullptr;
}

ff::value_ptr ff::value::try_convert(std::type_index type_index) const
{
    if (!this || this->is_type(type_index))
    {
        return this;
    }

    value_ptr new_val = this->type()->try_convert_to(this, type_index);
    if (!new_val)
    {
        const value_type* new_type = ff::value::get_type(type_index);
        if (new_type)
        {
            new_val = new_type->try_convert_from(this);
        }
    }

    return new_val;
}
