#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/idict.h"
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

bool ff_json_parse_idict(ff_string_view text, ff_idict* dict, ff_arena* arena, const char** error_pos)
{
    FF_ASSERT_RET_VAL(dict && arena, false);

    // The mutable dict is pure scratch: ff_idict_init copies every key, string and data payload
    // into the block it builds, so nothing in the finished idict points back here. A private heap
    // keeps that churn out of the caller's arena and frees it in one call, which matters because
    // parsing allocates well beyond what the result needs: dicts and arrays that grow leave their
    // old, smaller buffers stranded, and an arena never reclaims them.
    ff_arena scratch;
    ff_arena_init_heap_local(&scratch, 64 * 1024);

    ff_dict source;
    bool parsed = ff_json_parse(text, &source, &scratch, error_pos);

    if (parsed)
    {
        ff_idict_init(dict, arena, &source);
    }

    // Dropping the scratch is safe only because the idict owns its bytes outright.
    ff_arena_destroy(&scratch);

    return parsed;
}
