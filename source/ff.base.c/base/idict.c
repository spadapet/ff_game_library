#include "pch.h"
#include "base/hash.h"
#include "base/idict.h"
#include "base/ivalue.h"

static_assert(alignof(uint64_t) == alignof(ff_ivalue), "uint64_t and ff_ivalue must have the same alignment");

const uint64_t* internal_ff_idict_keys(const ff_idict* dict)
{
    return (const uint64_t*)dict->data.data;
}

const ff_ivalue* internal_ff_idict_values(const ff_idict* dict)
{
    return (const ff_ivalue*)(internal_ff_idict_keys(dict) + dict->count);
}

const void* internal_ff_idict_data(const ff_idict* dict, size_t offset)
{
    return (const uint8_t*)dict->data.data + dict->count * (sizeof(uint64_t) + sizeof(ff_ivalue)) + offset;
}

void ff_idict_init(ff_idict* dict, ff_arena* arena, ff_dict* source)
{
}

const ff_ivalue* ff_idict_get(const ff_idict* dict, ff_string_view key)
{
    return ff_idict_get_next(dict, key, NULL);
}

const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value)
{
    uint64_t key_hash = ff_hash_string(key);
    const uint64_t* keys = internal_ff_idict_keys(dict);
    const ff_ivalue* values = internal_ff_idict_values(dict);
    size_t count = dict->count;

    for (size_t i = prev_value ? (size_t)(prev_value - values) + 1 : 0; i < count; i++)
    {
        if (keys[i] == key_hash)
        {
            return values + i;
        }
    }

    return NULL;
}
