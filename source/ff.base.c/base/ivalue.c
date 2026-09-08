#include "pch.h"
#include "base/assert.h"
#include "base/dict_internal.h"
#include "base/idict.h"
#include "base/ivalue.h"

static_assert(sizeof(ff_ivalue) == sizeof(ff_value), "ff_value and ff_ivalue must stay layout-compatible");

ff_array_span ff_ivalue_as_data(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_array_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_data, result);

    result.count = value->data.count;
    result.data = internal_ff_idict_data(parent_dict, value->data.offset);
    result.item_size = value->data.item_size;
    result.item_align = value->data.item_align;
    return result;
}

ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_idict result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_dict, result);

    // A nested block carries its own count and size, so the offset is all that is needed.
    result.data = internal_ff_idict_data(parent_dict, value->data.offset);
    return result;
}

ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_string_view result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_string, result);

    result.data = (const char*)internal_ff_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}

ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict)
{
    ff_ivalue_span result = { 0 };
    FF_ASSERT_RET_VAL(value && parent_dict && parent_dict->data, result);
    FF_ASSERT_RET_VAL(value->type == ff_value_type_array, result);

    result.data = (const ff_ivalue*)internal_ff_idict_data(parent_dict, value->data.offset);
    result.count = value->data.count;
    return result;
}
