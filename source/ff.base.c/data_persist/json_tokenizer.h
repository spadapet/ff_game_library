#pragma once

#include "../base/string.h"
#include "../base/value.h"

typedef struct ff_arena ff_arena;

typedef enum ff_json_token_type
{
    ff_json_token_type_none, // end of text
    ff_json_token_type_error,
    ff_json_token_type_true,
    ff_json_token_type_false,
    ff_json_token_type_null,
    ff_json_token_type_string,
    ff_json_token_type_number,
    ff_json_token_type_comma,
    ff_json_token_type_colon,
    ff_json_token_type_open_curly,
    ff_json_token_type_close_curly,
    ff_json_token_type_open_bracket,
    ff_json_token_type_close_bracket,
} ff_json_token_type;

// 'text' points into the text being tokenized, so it must outlive the token. For strings it still
// has its quotes and escapes; ff_json_token_value is what decodes them.
typedef struct ff_json_token
{
    ff_string_view text;
    ff_json_token_type type;
    bool escaped; // strings only: false when the quoted text is already the final text
} ff_json_token;

typedef struct ff_json_tokenizer
{
    const char* pos;
    const char* end;
} ff_json_tokenizer;

void ff_json_tokenizer_init(ff_json_tokenizer* tokenizer, ff_string_view text);
ff_json_token ff_json_tokenizer_next(ff_json_tokenizer* tokenizer);

// Only converts tokens that stand alone: true, false, null, numbers and strings. Anything else,
// including a string with a bad escape, returns an empty value. Strings are copied into 'arena', so
// the value outlives the text that was tokenized.
ff_value ff_json_token_value(const ff_json_token* token, ff_arena* arena);

// The text of a string token, with the quotes removed and the escapes decoded. When the token has
// no escapes this points straight into the text being tokenized and nothing is allocated, so it is
// the cheap way to read a key or a string that is consumed right away. The result must not outlive
// the tokenized text; use ff_json_token_value when it has to. Other token types return an empty
// view.
ff_string_view ff_json_token_string(const ff_json_token* token, ff_arena* arena);
