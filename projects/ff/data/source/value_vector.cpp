#include "pch.h"
#include "value_vector.h"

ff::value_ptr ff::type::value_vector_type::try_convert_from(const value* other) const
{
    if (other->can_have_indexed_children())
    {
        ff::value_vector vec;
        vec.reserve(other->index_child_count());

        for (size_t i = 0; i < other->index_child_count(); i++)
        {
            vec.push_back(other->index_child(i));
        }

        return ff::value::create<ff::value_vector>(std::move(vec));
    }

    return nullptr;
}

bool ff::type::value_vector_type::can_have_indexed_children() const
{
    return true;
}

ff::value_ptr ff::type::value_vector_type::index_child(const value* val, size_t index) const
{
    const ff::value_vector& values = val->get<std::vector<value_ptr>>();
    return index < values.size() ? values[index] : nullptr;
}

size_t ff::type::value_vector_type::index_child_count(const value* val) const
{
    return val->get<ff::value_vector>().size();
}

ff::value_ptr ff::type::value_vector_type::load(reader_base& reader) const
{
    size_t size;
    if (ff::load(reader, size))
    {
        ff::value_vector vec;
        if (size)
        {
            vec.reserve(size);
            for (size_t i = 0; i < size; i++)
            {
                value_ptr child = ff::value::load_typed(reader);
                if (!child)
                {
                    assert(false);
                    return nullptr;
                }

                vec.push_back(std::move(child));
            }

            return ff::value::create<ff::value_vector>(std::move(vec));
        }
    }

    assert(false);
    return nullptr;
}

bool ff::type::value_vector_type::save(const value* val, writer_base& writer) const
{
    ff::value_vector src = val->get<ff::value_vector>();
    size_t size = src.size();
    if (ff::save(writer, size))
    {
        for (const value_ptr& child : src)
        {
            if (!child->save_typed(writer))
            {
                assert(false);
                return false;
            }
        }

        return true;
    }

    assert(false);
    return false;
}
