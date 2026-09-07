#include "pch.h"
#include "base/assert.h"
#include "base/idict.h"
#include "base/ivalue.h"

static_assert(sizeof(ff_ivalue) == sizeof(ff_value), "ff_value and ff_ivalue must stay layout-compatible");

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_dict);

    // TODO
    ff_idict result = { 0 };
    return result;
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_string);

    return (ff_string_view)
    {
        .data = (const char*)((const uint8_t*)base + value->data.offset),
        .count = value->data.count,
    };
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const void* base)
{
    FF_ASSERT(value->type == ff_value_type_array);

    return (ff_ivalue_span)
    {
        .data = (const ff_ivalue*)((const uint8_t*)base + value->data.offset),
        .count = value->data.count,
    };
}
