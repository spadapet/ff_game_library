#include "pch.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/assert.h"
#include "base/dict.h"
#include "base/idict.h"
#include "base/json.h"
#include "base/math.h"
#include "base/value.h"

// ====================================================================
// Tokenizer
//
// The parser below is the only consumer, so all of this is static and has no header.
// ====================================================================
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

static void ff_json_tokenizer_init(ff_json_tokenizer* tokenizer, ff_string_view text);
static ff_json_token ff_json_tokenizer_next(ff_json_tokenizer* tokenizer);
static ff_value ff_json_token_value(const ff_json_token* token, ff_arena* arena);
static ff_string_view ff_json_token_string(const ff_json_token* token, ff_arena* arena);

static bool is_json_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static bool is_json_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

static bool is_json_hex_digit(char ch)
{
    return is_json_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static bool is_json_alpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static char current_char(const ff_json_tokenizer* tokenizer)
{
    return tokenizer->pos < tokenizer->end ? *tokenizer->pos : '\0';
}

static char next_char(ff_json_tokenizer* tokenizer)
{
    return ++tokenizer->pos < tokenizer->end ? *tokenizer->pos : '\0';
}

static char peek_next_char(const ff_json_tokenizer* tokenizer)
{
    return tokenizer->pos + 1 < tokenizer->end ? tokenizer->pos[1] : '\0';
}

static bool skip_digits(ff_json_tokenizer* tokenizer, char* ch)
{
    FF_CHECK_RET_VAL(is_json_digit(*ch), false);

    do
    {
        *ch = next_char(tokenizer);
    }
    while (is_json_digit(*ch));

    return true;
}

static bool skip_identifier(ff_json_tokenizer* tokenizer, char* ch)
{
    FF_CHECK_RET_VAL(is_json_alpha(*ch), false);

    do
    {
        *ch = next_char(tokenizer);
    }
    while (is_json_alpha(*ch) || is_json_digit(*ch));

    return true;
}

static bool skip_number(ff_json_tokenizer* tokenizer, char* ch)
{
    if (*ch == '-')
    {
        *ch = next_char(tokenizer);
    }

    // JSON allows a single leading zero and nothing after it, so "01" is two tokens, not one.
    if (*ch == '0')
    {
        *ch = next_char(tokenizer);
        FF_CHECK_RET_VAL(!is_json_digit(*ch), false);
    }
    else
    {
        FF_CHECK_RET_VAL(skip_digits(tokenizer, ch), false);
    }

    if (*ch == '.')
    {
        *ch = next_char(tokenizer);
        FF_CHECK_RET_VAL(skip_digits(tokenizer, ch), false);
    }

    if (*ch == 'e' || *ch == 'E')
    {
        *ch = next_char(tokenizer);

        if (*ch == '-' || *ch == '+')
        {
            *ch = next_char(tokenizer);
        }

        FF_CHECK_RET_VAL(skip_digits(tokenizer, ch), false);
    }

    return true;
}

// Validates one UTF-8 sequence starting at 'pos' and returns its length, or 0 if it is not well
// formed. The ranges reject the things a naive decoder lets through: overlong encodings, which
// smuggle ASCII past filters, surrogates, which are not encodable, and anything past U+10FFFF.
static size_t utf8_sequence_length(const char* pos, const char* end)
{
    uint8_t lead = (uint8_t)*pos;

    if (lead < 0x80)
    {
        return 1;
    }

    size_t length;
    uint32_t code_point;

    if (lead >= 0xC2 && lead <= 0xDF)
    {
        length = 2;
        code_point = (uint32_t)(lead & 0x1F);
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        length = 3;
        code_point = (uint32_t)(lead & 0x0F);
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        length = 4;
        code_point = (uint32_t)(lead & 0x07);
    }
    else
    {
        // A continuation byte with no lead, or 0xC0/0xC1, which are only ever overlong.
        return 0;
    }

    if ((size_t)(end - pos) < length)
    {
        return 0;
    }

    for (size_t i = 1; i < length; i++)
    {
        uint8_t next = (uint8_t)pos[i];

        if ((next & 0xC0) != 0x80)
        {
            return 0;
        }

        code_point = (code_point << 6) | (uint32_t)(next & 0x3F);
    }

    // The lead byte alone cannot rule these out, so the decoded value is what decides.
    if (length == 3 && (code_point < 0x800 || (code_point >= 0xD800 && code_point <= 0xDFFF)))
    {
        return 0;
    }

    if (length == 4 && (code_point < 0x10000 || code_point > 0x10FFFF))
    {
        return 0;
    }

    return length;
}

// Only walks past the string, leaving the quotes and escapes in the token for the value conversion
// to deal with. Escapes are checked for shape here so the conversion can trust what it scans.
// '*escaped' is set when the string holds an escape, which is what lets a caller skip decoding.
static bool skip_string(ff_json_tokenizer* tokenizer, char* ch, bool* escaped)
{
    FF_CHECK_RET_VAL(*ch == '\"', false);
    *escaped = false;
    *ch = next_char(tokenizer);

    while (true)
    {
        // Plain ASCII is almost all of every real string, so it is walked without going back
        // through the per character helpers.
        const char* run = tokenizer->pos;

        while (run < tokenizer->end && (uint8_t)*run >= ' ' && (uint8_t)*run < 0x80 &&
            *run != '\"' && *run != '\\')
        {
            run++;
        }

        tokenizer->pos = run;
        *ch = current_char(tokenizer);

        if (*ch == '\"')
        {
            tokenizer->pos++;
            break;
        }
        else if (*ch == '\\')
        {
            *escaped = true;
            *ch = next_char(tokenizer);

            switch (*ch)
            {
                case '\"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    *ch = next_char(tokenizer);
                    break;

                case 'u':
                    FF_CHECK_RET_VAL(tokenizer->end - tokenizer->pos >= 5, false);
                    FF_CHECK_RET_VAL(
                        is_json_hex_digit(tokenizer->pos[1]) &&
                        is_json_hex_digit(tokenizer->pos[2]) &&
                        is_json_hex_digit(tokenizer->pos[3]) &&
                        is_json_hex_digit(tokenizer->pos[4]), false);

                    tokenizer->pos += 5;
                    *ch = current_char(tokenizer);
                    break;

                default:
                    // Includes '\0', so an escape at the end of the text is an error, not a loop.
                    return false;
            }
        }
        else if ((unsigned char)*ch < 0x80)
        {
            // A control character, or '\0' for an unterminated string: both are errors rather than
            // running off the end.
            return false;
        }
        else
        {
            // Checked here rather than left to the caller, so a string token is always known to be
            // valid UTF-8 and nothing downstream has to guess.
            size_t length = utf8_sequence_length(tokenizer->pos, tokenizer->end);
            FF_CHECK_RET_VAL(length, false);

            tokenizer->pos += length;
            *ch = current_char(tokenizer);
        }
    }

    return true;
}

static char skip_spaces_and_comments(ff_json_tokenizer* tokenizer)
{
    char ch = current_char(tokenizer);

    while (true)
    {
        if (is_json_space(ch))
        {
            ch = next_char(tokenizer);
        }
        else if (ch == '/' && peek_next_char(tokenizer) == '/')
        {
            tokenizer->pos++;
            ch = next_char(tokenizer);

            while (ch && ch != '\r' && ch != '\n')
            {
                ch = next_char(tokenizer);
            }
        }
        else if (ch == '/' && peek_next_char(tokenizer) == '*')
        {
            const char* start = tokenizer->pos++;
            ch = next_char(tokenizer);

            while (ch && (ch != '*' || peek_next_char(tokenizer) != '/'))
            {
                ch = next_char(tokenizer);
            }

            if (!ch)
            {
                // Unterminated, so rewind and let '/' be tokenized as the error it is.
                tokenizer->pos = start;
                return '/';
            }

            tokenizer->pos++;
            ch = next_char(tokenizer);
        }
        else
        {
            break;
        }
    }

    return ch;
}

static void ff_json_tokenizer_init(ff_json_tokenizer* tokenizer, ff_string_view text)
{
    tokenizer->pos = text.data;
    tokenizer->end = text.data + text.count;
}

static ff_json_token ff_json_tokenizer_next(ff_json_tokenizer* tokenizer)
{
    char ch = skip_spaces_and_comments(tokenizer);
    ff_json_token_type type = ff_json_token_type_error;
    const char* start = tokenizer->pos;
    bool escaped = false;

    switch (ch)
    {
        case '\0':
            type = ff_json_token_type_none;
            break;

        case 't':
            if (skip_identifier(tokenizer, &ch) && tokenizer->pos - start == 4 &&
                start[1] == 'r' && start[2] == 'u' && start[3] == 'e')
            {
                type = ff_json_token_type_true;
            }
            break;

        case 'f':
            if (skip_identifier(tokenizer, &ch) && tokenizer->pos - start == 5 &&
                start[1] == 'a' && start[2] == 'l' && start[3] == 's' && start[4] == 'e')
            {
                type = ff_json_token_type_false;
            }
            break;

        case 'n':
            if (skip_identifier(tokenizer, &ch) && tokenizer->pos - start == 4 &&
                start[1] == 'u' && start[2] == 'l' && start[3] == 'l')
            {
                type = ff_json_token_type_null;
            }
            break;

        case '\"':
            if (skip_string(tokenizer, &ch, &escaped))
            {
                type = ff_json_token_type_string;
            }
            break;

        case ',':
            tokenizer->pos++;
            type = ff_json_token_type_comma;
            break;

        case ':':
            tokenizer->pos++;
            type = ff_json_token_type_colon;
            break;

        case '{':
            tokenizer->pos++;
            type = ff_json_token_type_open_curly;
            break;

        case '}':
            tokenizer->pos++;
            type = ff_json_token_type_close_curly;
            break;

        case '[':
            tokenizer->pos++;
            type = ff_json_token_type_open_bracket;
            break;

        case ']':
            tokenizer->pos++;
            type = ff_json_token_type_close_bracket;
            break;

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (skip_number(tokenizer, &ch))
            {
                type = ff_json_token_type_number;
            }
            break;

        default:
            // An error token must still consume something, or the caller would spin forever.
            tokenizer->pos++;
            break;
    }

    ff_json_token token;
    token.type = type;
    token.text.data = start;
    token.text.count = (size_t)(tokenizer->pos - start);
    token.escaped = escaped;
    return token;
}

static uint32_t hex_digit_value(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return (uint32_t)(ch - '0');
    }

    return (uint32_t)((ch | 0x20) - 'a') + 10;
}

// Writes 'code_point' as UTF-8 and returns the byte count. Never more than 3 bytes, because a
// single \uXXXX escape cannot exceed 0xFFFF.
static size_t write_utf8(char* dest, uint32_t code_point)
{
    if (code_point < 0x80)
    {
        dest[0] = (char)code_point;
        return 1;
    }

    if (code_point < 0x800)
    {
        dest[0] = (char)(0xC0 | (code_point >> 6));
        dest[1] = (char)(0x80 | (code_point & 0x3F));
        return 2;
    }

    dest[0] = (char)(0xE0 | (code_point >> 12));
    dest[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
    dest[2] = (char)(0x80 | (code_point & 0x3F));
    return 3;
}

// The tokenizer already proved the escapes are well formed, so this only has to decode them. The
// decoded text is never longer than the quoted source, so one allocation up front is enough.
static ff_string_view json_string_text(ff_string_view text, bool escaped, ff_arena* arena)
{
    ff_string_view result;
    result.data = NULL;
    result.count = 0;

    FF_ASSERT_RET_VAL(text.count >= 2, result);

    if (!escaped)
    {
        // Nothing to decode, so the quoted text is already the answer and no copy is needed.
        result.data = text.data + 1;
        result.count = text.count - 2;
        return result;
    }

    const char* cur = text.data + 1;
    const char* end = text.data + text.count - 1;

    char* dest = ff_arena_alloc_type(arena, char, text.count);
    FF_ASSERT_RET_VAL(dest, result);
    size_t count = 0;

    while (cur < end)
    {
        if (*cur != '\\')
        {
            dest[count++] = *cur++;
            continue;
        }

        switch (cur[1])
        {
            case '\"': dest[count++] = '\"'; break;
            case '\\': dest[count++] = '\\'; break;
            case '/': dest[count++] = '/'; break;
            case 'b': dest[count++] = '\b'; break;
            case 'f': dest[count++] = '\f'; break;
            case 'n': dest[count++] = '\n'; break;
            case 'r': dest[count++] = '\r'; break;
            case 't': dest[count++] = '\t'; break;

            case 'u':
                {
                    uint32_t code_point =
                        (hex_digit_value(cur[2]) << 12) |
                        (hex_digit_value(cur[3]) << 8) |
                        (hex_digit_value(cur[4]) << 4) |
                        hex_digit_value(cur[5]);

                    // A lone surrogate is not encodable, so it becomes the replacement character
                    // rather than producing bytes no UTF-8 reader would accept.
                    if (code_point >= 0xD800 && code_point <= 0xDBFF &&
                        end - cur >= 12 && cur[6] == '\\' && cur[7] == 'u')
                    {
                        uint32_t low =
                            (hex_digit_value(cur[8]) << 12) |
                            (hex_digit_value(cur[9]) << 8) |
                            (hex_digit_value(cur[10]) << 4) |
                            hex_digit_value(cur[11]);

                        if (low >= 0xDC00 && low <= 0xDFFF)
                        {
                            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);

                            dest[count++] = (char)(0xF0 | (code_point >> 18));
                            dest[count++] = (char)(0x80 | ((code_point >> 12) & 0x3F));
                            dest[count++] = (char)(0x80 | ((code_point >> 6) & 0x3F));
                            dest[count++] = (char)(0x80 | (code_point & 0x3F));

                            cur += 12;
                            continue;
                        }
                    }

                    if (code_point >= 0xD800 && code_point <= 0xDFFF)
                    {
                        code_point = 0xFFFD;
                    }

                    count += write_utf8(dest + count, code_point);
                    cur += 6;
                }
                continue;

            default:
                // The tokenizer rejects anything else, so reaching here means the token is not the
                // one that was scanned.
                FF_DEBUG_FAIL();
                return result;
        }

        cur += 2;
    }

    result.data = dest;
    result.count = count;
    return result;
}

// Integers are read directly so large 64 bit values keep every digit, which going through a double
// would quietly round away.
static ff_value json_number_value(ff_string_view text)
{
    FF_ASSERT_RET_VAL(text.count, ff_value_new_empty());

    size_t i = 0;
    bool negative = text.data[0] == '-';
    i += negative ? 1 : 0;

    bool is_integer = true;
    uint64_t magnitude = 0;
    bool overflow = false;

    for (; i < text.count; i++)
    {
        char ch = text.data[i];

        if (!is_json_digit(ch))
        {
            // '.', 'e' or 'E': the tokenizer allows nothing else.
            is_integer = false;
            break;
        }

        uint64_t digit = (uint64_t)(ch - '0');
        overflow = overflow || magnitude > (UINT64_MAX - digit) / 10;
        magnitude = magnitude * 10 + digit;
    }

    if (is_integer && !overflow)
    {
        uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;

        if (magnitude <= limit)
        {
            // Negating through uint64 so INT64_MIN, whose magnitude has no positive counterpart,
            // still round trips.
            int64_t value = negative ? (int64_t)(0 - magnitude) : (int64_t)magnitude;

            return (value >= INT32_MIN && value <= INT32_MAX)
                ? ff_value_new_int32((int32_t)value)
                : ff_value_new_int64(value);
        }
    }

    // strtod needs a terminator, and the token is a slice of a larger text. Every number the
    // tokenizer accepts fits easily, and anything longer is pathological padding.
    char buffer[512];
    FF_CHECK_RET_VAL(text.count < sizeof(buffer), ff_value_new_empty());
    memcpy(buffer, text.data, text.count);
    buffer[text.count] = '\0';

    char* parse_end = NULL;
    double value = strtod(buffer, &parse_end);
    FF_CHECK_RET_VAL(parse_end == buffer + text.count, ff_value_new_empty());

    // Overflow to infinity is a rejection, not a result: JSON has no way to write infinity back
    // out, so accepting it would produce a value that cannot round trip. Underflow to zero is
    // allowed, because that is ordinary loss of precision rather than a value out of range.
    FF_CHECK_RET_VAL(!isinf(value), ff_value_new_empty());

    return ff_value_new_float64(value);
}

static ff_string_view ff_json_token_string(const ff_json_token* token, ff_arena* arena)
{
    ff_string_view result;
    result.data = NULL;
    result.count = 0;

    FF_ASSERT_RET_VAL(token, result);
    FF_CHECK_RET_VAL(token->type == ff_json_token_type_string, result);

    return json_string_text(token->text, token->escaped, arena);
}

static ff_value ff_json_token_value(const ff_json_token* token, ff_arena* arena)
{
    FF_ASSERT_RET_VAL(token, ff_value_new_empty());

    switch (token->type)
    {
        case ff_json_token_type_true:
            return ff_value_new_boolean(true);

        case ff_json_token_type_false:
            return ff_value_new_boolean(false);

        case ff_json_token_type_null:
            return ff_value_new_null();

        case ff_json_token_type_number:
            return json_number_value(token->text);

        case ff_json_token_type_string:
            {
                ff_string_view text = json_string_text(token->text, token->escaped, arena);
                FF_CHECK_RET_VAL(text.data, ff_value_new_empty());

                if (!token->escaped)
                {
                    // A borrowed view points into the text being tokenized, but a value has to
                    // outlive it, so this is the one place that always takes a copy.
                    char* copy = ff_arena_alloc_type(arena, char, text.count ? text.count : 1);
                    FF_ASSERT_RET_VAL(copy, ff_value_new_empty());
                    memcpy(copy, text.data, text.count);
                    text.data = copy;
                }

                return ff_value_new_string(text);
            }

        default:
            return ff_value_new_empty();
    }
}

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

        // Only the key's hash is kept, so this takes the borrowed view, which allocates nothing
        // unless the key has escapes. ff_json_token_value would copy the text just to throw it away.
        ff_string_view key = ff_json_token_string(&token, parser->arena);

        if (!key.data)
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

        // Duplicate keys are all kept, and a lookup finds the first one that was parsed. This is a
        // deliberate departure from the usual JSON convention of the last value winning (RFC 8259
        // leaves the choice to the implementation). ff_dict_add avoids the scan through every
        // existing entry that ff_dict_set needs in order to delete an earlier duplicate.
        ff_dict_add(dict, key, &value);

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
    dict->data = NULL;

    ff_arena scratch;
    ff_arena_init_heap_global(&scratch, 64 * 1024);

    ff_dict source;
    bool parsed = ff_json_parse(text, &source, &scratch, error_pos);
    if (parsed)
    {
        ff_idict_init(dict, arena, &source);
    }

    ff_arena_destroy(&scratch);

    return parsed;
}
