#include "pch.h"
#include "base/assert.h"
#include "base/idict.h"
#include "base/ivalue.h"

static_assert(sizeof(ff_ivalue) == sizeof(ff_value), "ff_value and ff_ivalue must stay layout-compatible");

const void* internal_ff_idict_data(const ff_idict* dict, size_t offset);

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict)
{
    FF_ASSERT(value->type == ff_value_type_dict);

    return (ff_idict)
    {
        .count = value->data.item_size,
        .data = (ff_span)
        {
            .data = internal_ff_idict_data(parent_dict, value->data.offset),
            .size = value->data.count,
        },
    };
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict)
{
    FF_ASSERT(value->type == ff_value_type_string);

    return (ff_string_view)
    {
        .data = internal_ff_idict_data(parent_dict, value->data.offset),
        .count = value->data.count,
    };
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict)
{
    FF_ASSERT(value->type == ff_value_type_array);

    return (ff_ivalue_span)
    {
        .data = internal_ff_idict_data(parent_dict, value->data.offset),
        .count = value->data.count,
    };
}
