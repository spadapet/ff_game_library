#pragma once

#include "../base/span.h"
#include "../base/string.h"
#include "../base/value_type.h"

typedef struct ff_arena ff_arena;
typedef struct ff_dict ff_dict;
typedef struct ff_value ff_value;

// Every block, and every nested dict inside one, starts at this alignment.
#define FF_IDICT_MAX_ALIGN 64

typedef struct ff_idict
{
    const void* data;
} ff_idict;

typedef struct ff_ivalue
{
    union
    {
        bool b;
        GUID guid;

        int32_t i32;
        int64_t i64;
        float f32;
        double f64;

        int32_t point_i32[2];
        float point_f32[2];
        int64_t point_i64[2];
        double point_f64[2];

        int32_t rect_i32[4];
        float rect_f32[4];

        struct ff_array_slice data;
    };

    ff_value_type type;
} ff_ivalue;

typedef struct ff_ivalue_span
{
    const ff_ivalue* data;
    size_t count;
} ff_ivalue_span;

void ff_idict_init(ff_idict* dict, ff_arena* arena, const ff_dict* source);

// Writes an idict block directly, for callers like the JSON parser that already know how many
// entries each dict has and can emit them in one pass instead of building an ff_dict first. Every
// offset inside a block is relative to the block, so the buffer can be grown and relocated freely
// while it is being written.
typedef struct ff_idict_builder
{
    ff_arena* data_arena; // grows the block, so the block must stay its newest allocation
    ff_arena* scratch_arena;
    uint8_t* data;
    size_t byte_size;
    size_t byte_capacity;
} ff_idict_builder;

void ff_idict_builder_init(ff_idict_builder* builder, ff_arena* data_arena, ff_arena* scratch_arena, size_t initial_capacity);
void ff_idict_builder_finish(ff_idict_builder* builder, ff_idict* dict);

// 'entry_count' must be exact. Entries are set by index in any order, then the dict is closed,
// which sorts them by key hash. Returns the block offset that identifies this dict.
size_t ff_idict_builder_open_dict(ff_idict_builder* builder, size_t entry_count);
void ff_idict_builder_set_entry(ff_idict_builder* builder, size_t block_offset, size_t entry_count, size_t index, ff_string_view key, const ff_ivalue* value);
void ff_idict_builder_close_dict(ff_idict_builder* builder, size_t block_offset, size_t entry_count);
// The offset of a dict's data section, which every value inside that dict is relative to.
size_t ff_idict_builder_data_offset(size_t block_offset, size_t entry_count);

// Reserves room for 'item_count' items and returns their offset. Items are set by index.
size_t ff_idict_builder_open_array(ff_idict_builder* builder, size_t item_count);
void ff_idict_builder_set_item(ff_idict_builder* builder, size_t items_offset, size_t index, const ff_ivalue* value);

// Copies any payload 'value' owns into the block. 'data_offset' is the owning dict's data offset.
void ff_idict_builder_value(ff_idict_builder* builder, const ff_value* value, size_t data_offset, ff_ivalue* result);
// Makes the value that refers to an already written array or nested dict.
ff_ivalue ff_idict_builder_array_value(size_t items_offset, size_t item_count, size_t data_offset);
ff_ivalue ff_idict_builder_dict_value(size_t block_offset, size_t data_offset);
const ff_ivalue* ff_idict_get(const ff_idict* dict, ff_string_view key);
const ff_ivalue* ff_idict_get_next(const ff_idict* dict, ff_string_view key, const ff_ivalue* prev_value);
ff_span ff_idict_save(const ff_idict* dict, ff_arena* arena);
// The saved bytes are used in place, so they must outlive the dict and be 64 byte aligned.
bool ff_idict_load(ff_idict* dict, ff_span saved, bool validate_values, bool validate_hash);

ff_array_span ff_ivalue_as_data(const ff_ivalue* value, const ff_idict* parent_dict);
ff_idict ff_ivalue_as_dict(const ff_ivalue* value, const ff_idict* parent_dict);
ff_string_view ff_ivalue_as_string(const ff_ivalue* value, const ff_idict* parent_dict);
ff_ivalue_span ff_ivalue_as_array(const ff_ivalue* value, const ff_idict* parent_dict);
