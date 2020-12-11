#pragma once

#include "value/value_ptr.h"

namespace ff::internal
{
    enum class json_token_type
    {
        none,
        error,
        true_token,
        false_token,
        null_token,
        string_token,
        number_token,
        comma,
        colon,
        open_curly,
        close_curly,
        open_bracket,
        close_bracket,
    };

    struct json_token
    {
        value_ptr get() const;
        const char* begin() const;
        const char* end() const;
        size_t size() const;

        json_token_type type;
        std::string_view text;
    };

    class json_tokenizer
    {
    public:
        json_tokenizer(std::string_view text);

        json_token next();

    private:
        bool skip_string(char& ch);
        bool skip_number(char& ch);
        bool skip_digits(char& ch);
        bool skip_identifier(char& ch);
        char skip_spaces_and_comments(char ch);
        char current_char() const;
        char next_char();
        char peek_next_char();

        const char* pos;
        const char* end;
    };
}
