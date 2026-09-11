#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/idict.h"
#include "base/math.h"
#include "base/value.h"
#include "data_persist/json_persist.h"
#include "data_persist/json_tokenizer.h"

typedef struct internal_ff_json_parser
{
    ff_json_tokenizer tokenizer;
    ff_arena* arena;
    const char* error_pos;
} internal_ff_json_parser;

static void parse_error(internal_ff_json_parser* parser, const ff_json_token* token)
{
    if (!parser->error_pos)
    {
        parser->error_pos = token->text.data;
    }
}

static bool parse_value(internal_ff_json_parser* parser, const ff_json_token* token, ff_value* result);

// 'open' has already been consumed. Items land straight in a growable array, whose storage becomes
// the array value's storage, so the values are never copied.
static bool parse_array(internal_ff_json_parser* parser, ff_value* result)
{
    ff_value* items_a = ff_array_init(ff_value, parser->arena);
    ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

    while (token.type != ff_json_token_type_close_bracket)
    {
        ff_value item;

        if (!parse_value(parser, &token, &item))
        {
            return false;
        }

        ff_array_push(items_a, item);

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);

            // A comma must be followed by another item, so a trailing one is an error.
            if (token.type == ff_json_token_type_close_bracket)
            {
                parse_error(parser, &token);
                return false;
            }
        }
        else if (token.type != ff_json_token_type_close_bracket)
        {
            parse_error(parser, &token);
            return false;
        }
    }

    ff_value_span span;
    span.data = items_a;
    span.count = ff_array_count(items_a);

    *result = ff_value_new_array(span);
    return true;
}

// 'open' has already been consumed.
static bool parse_object(internal_ff_json_parser* parser, ff_dict* dict)
{
    ff_dict_init(dict, parser->arena);

    ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

    while (token.type != ff_json_token_type_close_curly)
    {
        if (token.type != ff_json_token_type_string)
        {
            parse_error(parser, &token);
            return false;
        }

        ff_value key = ff_json_token_value(&token, parser->arena);

        if (key.type != ff_value_type_string)
        {
            parse_error(parser, &token);
            return false;
        }

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type != ff_json_token_type_colon)
        {
            parse_error(parser, &token);
            return false;
        }

        ff_json_token value_token = ff_json_tokenizer_next(&parser->tokenizer);
        ff_value value;

        if (!parse_value(parser, &value_token, &value))
        {
            return false;
        }

        // Duplicate keys must collapse to the last one (RFC 8259 leaves this to the implementation,
        // and last-wins matches both the old C++ parser and what most JSON parsers do). That is why
        // this uses ff_dict_set and not the cheaper ff_dict_add: set removes any earlier entry with
        // the same key, while add would leave both and let ff_dict_get find the first.
        ff_dict_set(dict, ff_value_as_string(&key), &value);

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);

            if (token.type == ff_json_token_type_close_curly)
            {
                parse_error(parser, &token);
                return false;
            }
        }
        else if (token.type != ff_json_token_type_close_curly)
        {
            parse_error(parser, &token);
            return false;
        }
    }

    return true;
}

static bool parse_value(internal_ff_json_parser* parser, const ff_json_token* token, ff_value* result)
{
    if (token->type == ff_json_token_type_open_curly)
    {
        ff_dict* dict = ff_arena_alloc_type(parser->arena, ff_dict, 1);
        FF_ASSERT_RET_VAL(dict, false);

        FF_CHECK_RET_VAL(parse_object(parser, dict), false);

        *result = ff_value_new_dict(dict);
        return true;
    }

    if (token->type == ff_json_token_type_open_bracket)
    {
        return parse_array(parser, result);
    }

    *result = ff_json_token_value(token, parser->arena);

    if (result->type == ff_value_type_empty)
    {
        parse_error(parser, token);
        return false;
    }

    return true;
}

bool ff_json_parse(ff_string_view text, ff_dict* dict, ff_arena* arena, const char** error_pos)
{
    const char* ignored = NULL;
    error_pos = error_pos ? error_pos : &ignored;
    *error_pos = NULL;

    FF_ASSERT_RET_VAL(dict && arena, false);

    internal_ff_json_parser parser;
    parser.arena = arena;
    parser.error_pos = NULL;
    ff_json_tokenizer_init(&parser.tokenizer, text);

    ff_dict_init(dict, arena);

    ff_json_token token = ff_json_tokenizer_next(&parser.tokenizer);

    if (token.type != ff_json_token_type_open_curly)
    {
        *error_pos = token.text.data;
        return false;
    }

    if (!parse_object(&parser, dict))
    {
        *error_pos = parser.error_pos;
        return false;
    }

    // Anything after the root object means the text is not one document.
    token = ff_json_tokenizer_next(&parser.tokenizer);

    if (token.type != ff_json_token_type_none)
    {
        *error_pos = token.text.data;
        return false;
    }

    return true;
}

// An idict is written straight out, with no ff_dict in between, by going over the text twice. The
// first pass only validates and counts, because a dict's data section starts at a fixed distance
// past its entry table, so nothing inside it can be placed until the entry count is known. The
// counts come out in document order, and the second pass opens the same aggregates in the same
// order, so one advancing index is enough to look each count back up.

typedef struct internal_ff_json_idict_parser
{
    ff_json_tokenizer tokenizer;
    ff_arena* scratch;
    size_t* counts_a;
    size_t count_index; // second pass only
    size_t byte_estimate; // first pass only
    ff_idict_builder builder;
    const char* error_pos;
} internal_ff_json_idict_parser;

static bool count_error(internal_ff_json_idict_parser* parser, const ff_json_token* token)
{
    if (!parser->error_pos)
    {
        parser->error_pos = token->text.data;
    }

    return false;
}

static bool count_value(internal_ff_json_idict_parser* parser, const ff_json_token* token);

static bool count_array(internal_ff_json_idict_parser* parser)
{
    size_t slot = ff_array_count(parser->counts_a);
    ff_array_push(parser->counts_a, (size_t)0);

    size_t count = 0;
    ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

    while (token.type != ff_json_token_type_close_bracket)
    {
        FF_CHECK_RET_VAL(count_value(parser, &token), false);
        count++;

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);
            if (token.type == ff_json_token_type_close_bracket)
            {
                return count_error(parser, &token);
            }
        }
        else
        {
            if (token.type != ff_json_token_type_close_bracket)
            {
                return count_error(parser, &token);
            }
        }
    }

    parser->counts_a[slot] = count;

    // The items, plus the padding that may precede them.
    parser->byte_estimate += count * sizeof(ff_ivalue) + alignof(ff_ivalue);
    return true;
}

static bool count_object(internal_ff_json_idict_parser* parser)
{
    size_t slot = ff_array_count(parser->counts_a);
    ff_array_push(parser->counts_a, (size_t)0);

    size_t count = 0;
    ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

    while (token.type != ff_json_token_type_close_curly)
    {
        if (token.type != ff_json_token_type_string)
        {
            return count_error(parser, &token);
        }

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type != ff_json_token_type_colon)
        {
            return count_error(parser, &token);
        }

        ff_json_token value_token = ff_json_tokenizer_next(&parser->tokenizer);
        FF_CHECK_RET_VAL(count_value(parser, &value_token), false);
        count++;

        token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);
            if (token.type == ff_json_token_type_close_curly)
            {
                return count_error(parser, &token);
            }
        }
        else
        {
            if (token.type != ff_json_token_type_close_curly)
            {
                return count_error(parser, &token);
            }
        }
    }

    parser->counts_a[slot] = count;

    // The entry table, plus the padding that pushes it and its data section to their alignment.
    parser->byte_estimate += 8 + count * (sizeof(uint64_t) + sizeof(ff_ivalue)) + 2 * FF_IDICT_MAX_ALIGN;
    return true;
}

static bool count_value(internal_ff_json_idict_parser* parser, const ff_json_token* token)
{
    switch (token->type)
    {
        case ff_json_token_type_open_curly:
            return count_object(parser);

        case ff_json_token_type_open_bracket:
            return count_array(parser);

        default:
            {
                if (token->type == ff_json_token_type_string)
                {
                    // Only strings reach the data section, and never more bytes than the quoted
                    // text, since escapes only ever shrink. They are byte aligned, so no padding.
                    parser->byte_estimate += token->text.count;
                    return true;
                }

                if (token->type == ff_json_token_type_number)
                {
                    // Numbers live entirely inside the entry, so they add nothing to the data
                    // section, but they still have to be proven convertible.
                    ff_value value = ff_json_token_value(token, parser->scratch);
                    return value.type != ff_value_type_empty || count_error(parser, token);
                }

                return token->type == ff_json_token_type_true ||
                    token->type == ff_json_token_type_false ||
                    token->type == ff_json_token_type_null ||
                    count_error(parser, token);
            }
    }
}

// The second pass trusts the text, because the first pass already rejected anything malformed.
static void emit_value(internal_ff_json_idict_parser* parser, const ff_json_token* token, size_t data_offset, ff_ivalue* result);

static void emit_array(internal_ff_json_idict_parser* parser, size_t data_offset, ff_ivalue* result)
{
    size_t count = parser->counts_a[parser->count_index++];
    size_t items_offset = ff_idict_builder_open_array(&parser->builder, count);

    for (size_t i = 0; i < count; i++)
    {
        ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);
        }

        ff_ivalue item;
        emit_value(parser, &token, data_offset, &item);
        ff_idict_builder_set_item(&parser->builder, items_offset, i, &item);
    }

    ff_json_tokenizer_next(&parser->tokenizer); // ']'

    *result = ff_idict_builder_array_value(items_offset, count, data_offset);
}

static size_t emit_object(internal_ff_json_idict_parser* parser)
{
    size_t count = parser->counts_a[parser->count_index++];
    size_t block_offset = ff_idict_builder_open_dict(&parser->builder, count);
    size_t data_offset = ff_idict_builder_data_offset(block_offset, count);

    for (size_t i = 0; i < count; i++)
    {
        ff_json_token token = ff_json_tokenizer_next(&parser->tokenizer);

        if (token.type == ff_json_token_type_comma)
        {
            token = ff_json_tokenizer_next(&parser->tokenizer);
        }

        ff_arena_marker marker = ff_arena_mark(parser->scratch);
        ff_string_view key = ff_json_token_string(&token, parser->scratch);

        ff_json_tokenizer_next(&parser->tokenizer); // ':'

        ff_json_token value_token = ff_json_tokenizer_next(&parser->tokenizer);
        ff_ivalue value;
        emit_value(parser, &value_token, data_offset, &value);

        ff_idict_builder_set_entry(&parser->builder, block_offset, count, i, key, &value);
        ff_arena_rewind(parser->scratch, marker);
    }

    ff_json_tokenizer_next(&parser->tokenizer); // '}'

    ff_idict_builder_close_dict(&parser->builder, block_offset, count);
    return block_offset;
}

static void emit_value(internal_ff_json_idict_parser* parser, const ff_json_token* token, size_t data_offset, ff_ivalue* result)
{
    if (token->type == ff_json_token_type_open_curly)
    {
        *result = ff_idict_builder_dict_value(emit_object(parser), data_offset);
        return;
    }

    if (token->type == ff_json_token_type_open_bracket)
    {
        emit_array(parser, data_offset, result);
        return;
    }

    ff_arena_marker marker = ff_arena_mark(parser->scratch);
    ff_value value;

    if (token->type == ff_json_token_type_string)
    {
        // An unescaped string is borrowed straight from the text, so this usually allocates nothing.
        value = ff_value_new_string(ff_json_token_string(token, parser->scratch));
    }
    else
    {
        value = ff_json_token_value(token, parser->scratch);
    }

    ff_idict_builder_value(&parser->builder, &value, data_offset, result);
    ff_arena_rewind(parser->scratch, marker);
}

bool ff_json_parse_idict(ff_string_view text, ff_idict* dict, ff_arena* arena, const char** error_pos)
{
    const char* ignored = NULL;
    error_pos = error_pos ? error_pos : &ignored;
    *error_pos = NULL;

    FF_ASSERT_RET_VAL(dict && arena, false);

    dict->data = NULL;

    // A private heap keeps the counting pass and the decoded strings out of the caller's arena, and
    // frees all of it in one call.
    ff_arena scratch;
    ff_arena_init_heap_local(&scratch, 64 * 1024);

    internal_ff_json_idict_parser parser;
    parser.scratch = &scratch;
    parser.counts_a = ff_array_init(size_t, &scratch);
    parser.count_index = 0;
    parser.byte_estimate = 0;
    parser.error_pos = NULL;
    ff_json_tokenizer_init(&parser.tokenizer, text);

    ff_json_token token = ff_json_tokenizer_next(&parser.tokenizer);
    bool parsed = true;

    if (token.type != ff_json_token_type_open_curly)
    {
        parser.error_pos = token.text.data;
        parsed = false;
    }
    else if (!count_object(&parser))
    {
        parsed = false;
    }
    else
    {
        // Anything after the root object means the text is not one document.
        ff_json_token extra = ff_json_tokenizer_next(&parser.tokenizer);

        if (extra.type != ff_json_token_type_none)
        {
            parser.error_pos = extra.text.data;
            parsed = false;
        }
    }

    if (parsed)
    {
        // The counting pass added up an upper bound, so the block is allocated once and the
        // unused tail is handed back at the end.
        ff_idict_builder_init(&parser.builder, arena, &scratch, parser.byte_estimate);
        ff_json_tokenizer_init(&parser.tokenizer, text);
        ff_json_tokenizer_next(&parser.tokenizer); // '{'
        emit_object(&parser);
        ff_idict_builder_finish(&parser.builder, dict);
    }
    else
    {
        *error_pos = parser.error_pos;
    }

    ff_arena_destroy(&scratch);

    return parsed;
}
