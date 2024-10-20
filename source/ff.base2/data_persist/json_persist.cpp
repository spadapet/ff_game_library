#include "pch.h"
#include "base/assert.h"
#include "data_persist/json_persist.h"
#include "data_persist/json_tokenizer.h"
#include "data_value/bool_v.h"
#include "data_value/dict_v.h"
#include "data_value/double_v.h"
#include "data_value/int_v.h"
#include "data_value/null_v.h"
#include "data_value/saved_data_v.h"
#include "data_value/string_v.h"
#include "data_value/value_vector_v.h"

constexpr size_t INDENT_SPACES = 2;

static bool json_write_value(const ff::value* value, size_t spaces, std::ostream& output);
static void json_write_object(const ff::dict& dict, size_t spaces, std::ostream& output);
static ff::dict parse_object(ff::internal::json_tokenizer& tokenizer, const char** error_pos);
static ff::value_vector parse_array(ff::internal::json_tokenizer& tokenizer, const char** error_pos);

static ff::value_ptr parse_value(ff::internal::json_tokenizer& tokenizer, const ff::internal::json_token* first_token, const char** error_pos)
{
    ff::internal::json_token token = first_token ? *first_token : tokenizer.next();
    ff::value_ptr value = token.get();

    if (!value)
    {
        if (token.type == ff::internal::json_token_type::open_curly)
        {
            // Nested object
            ff::dict value_dict = ::parse_object(tokenizer, error_pos);
            if (!*error_pos)
            {
                value = ff::value::create<ff::dict>(std::move(value_dict));
            }
        }
        else if (token.type == ff::internal::json_token_type::open_bracket)
        {
            // Array
            ff::value_vector value_vector = ::parse_array(tokenizer, error_pos);
            if (!*error_pos)
            {
                value = ff::value::create<ff::value_vector>(std::move(value_vector));
            }
        }
    }

    if (!*error_pos && !value)
    {
        *error_pos = token.begin();
    }

    return value;
}

static ff::value_vector parse_array(ff::internal::json_tokenizer& tokenizer, const char** error_pos)
{
    ff::value_vector values;

    for (ff::internal::json_token token = tokenizer.next(); token.type != ff::internal::json_token_type::close_bracket; )
    {
        ff::value_ptr value = ::parse_value(tokenizer, &token, error_pos);
        if (*error_pos)
        {
            break;
        }

        values.push_back(value);

        token = tokenizer.next();
        if (token.type != ff::internal::json_token_type::comma &&
            token.type != ff::internal::json_token_type::close_bracket)
        {
            *error_pos = token.begin();
            break;
        }

        if (token.type == ff::internal::json_token_type::comma)
        {
            token = tokenizer.next();
        }
    }

    return values;
}

static ff::dict parse_object(ff::internal::json_tokenizer& tokenizer, const char** error_pos)
{
    ff::dict dict;

    for (ff::internal::json_token token = tokenizer.next(); token.type != ff::internal::json_token_type::close_curly; )
    {
        // Pair name is first
        if (token.type != ff::internal::json_token_type::string_token)
        {
            *error_pos = token.begin();
            break;
        }

        // Get string from token
        ff::value_ptr key = token.get();
        if (!key)
        {
            *error_pos = token.begin();
            break;
        }

        // Colon must be after name
        token = tokenizer.next();
        if (token.type != ff::internal::json_token_type::colon)
        {
            *error_pos = token.begin();
            break;
        }

        ff::value_ptr value = ::parse_value(tokenizer, nullptr, error_pos);
        if (*error_pos)
        {
            break;
        }

        dict.set(key->get<std::string>(), value);

        token = tokenizer.next();
        if (token.type != ff::internal::json_token_type::comma &&
            token.type != ff::internal::json_token_type::close_curly)
        {
            *error_pos = token.begin();
            break;
        }

        if (token.type == ff::internal::json_token_type::comma)
        {
            token = tokenizer.next();
        }
    }

    return dict;
}

static ff::dict parse_root_object(ff::internal::json_tokenizer& tokenizer, const char** error_pos)
{
    ff::internal::json_token token = tokenizer.next();
    if (token.type == ff::internal::json_token_type::open_curly)
    {
        return ::parse_object(tokenizer, error_pos);
    }

    *error_pos = token.begin();
    return ff::dict();
}

bool ff::json_parse(std::string_view text, ff::dict& dict, const char** error_pos)
{
    ff::internal::json_tokenizer tokenizer(text);

    const char* my_error_pos;
    error_pos = error_pos ? error_pos : &my_error_pos;
    *error_pos = nullptr;

    dict = ::parse_root_object(tokenizer, error_pos);
    return !*error_pos;
}

static void json_encode(std::string_view text, std::ostream& output)
{
    output << '\"';

    for (char ch : text)
    {
        switch (ch)
        {
            case '\"':
                output << "\\\"";
                break;

            case '\\':
                output << "\\\\";
                break;

            case '\b':
                output << "\\b";
                break;

            case '\f':
                output << "\\f";
                break;

            case '\n':
                output << "\\n";
                break;

            case '\r':
                output << "\\r";
                break;

            case '\t':
                output << "\\t";
                break;

            default:
                if (ch >= ' ')
                {
                    output << ch;
                }
                break;
        }
    }

    output << '\"';
}

static void json_write_array(const ff::value_vector& values, size_t spaces, std::ostream& output)
{
    output << '[';

    size_t size = values.size();
    if (size)
    {
        output << "\r\n";
        std::string indent(spaces + ::INDENT_SPACES, ' ');

        for (size_t i = 0; i < size; i++)
        {
            output << indent;

            ::json_write_value(values[i], spaces + ::INDENT_SPACES, output);

            if (i + 1 < size)
            {
                output << ',';
            }

            output << "\r\n";
        }

        output << std::string(spaces, ' ');
    }

    output << ']';
}

static bool json_write_value(const ff::value* value, size_t spaces, std::ostream& output)
{
    if (value->is_type<std::string>())
    {
        output << ' ';
        ::json_encode(value->get<std::string>(), output);
    }
    else if (value->is_type<bool>() ||
        value->is_type<nullptr_t>() ||
        value->is_type<double>() ||
        value->is_type<int>())
    {
        ff::value_ptr str_value = value->convert_or_default<std::string>();
        output << ' ' << str_value->get<std::string>();
    }
    else if (value->is_type<ff::dict>() || value->is_type<ff::saved_data_base>())
    {
        ff::value_ptr dict_value = value->convert_or_default<ff::dict>();
        if (dict_value->get<ff::dict>().size())
        {
            output << "\r\n" << std::string(spaces, ' ');
        }
        else
        {
            output << ' ';
        }

        ::json_write_object(dict_value->get<ff::dict>(), spaces, output);
    }
    else if (value->is_type<ff::value_vector>())
    {
        if (value->get<ff::value_vector>().size())
        {
            output << "\r\n" << std::string(spaces, ' ');
        }
        else
        {
            output << ' ';
        }

        ::json_write_array(value->get<ff::value_vector>(), spaces, output);
    }
    else
    {
        output << "null";
        assert(false);
        return false;
    }

    return true;
}

static void json_write_object(const ff::dict& dict, size_t spaces, std::ostream& output)
{
    output << '{';

    std::vector<std::string_view> names = dict.child_names();
    std::sort(names.begin(), names.end());

    if (!names.empty())
    {
        output << "\r\n";
        std::string indent(spaces + INDENT_SPACES, ' ');

        for (size_t i = 0; i < names.size(); i++)
        {
            output << indent;

            // "key": value,
            ::json_encode(names[i], output);
            output << ':';
            ::json_write_value(dict.get(names[i]), spaces + INDENT_SPACES, output);

            if (i + 1 < names.size())
            {
                output << ',';
            }

            output << "\r\n";
        }

        output << std::string(spaces, ' ');
    }

    output << '}';
}

void ff::json_write(const ff::dict& dict, std::ostream& output)
{
    ::json_write_object(dict, 0, output);
}
