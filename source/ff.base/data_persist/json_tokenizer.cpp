#include "pch.h"
#include "data_persist/json_tokenizer.h"
#include "data_value/bool_v.h"
#include "data_value/double_v.h"
#include "data_value/int_v.h"
#include "data_value/null_v.h"
#include "data_value/string_v.h"

ff::value_ptr ff::internal::json_token::get() const
{
    switch (this->type)
    {
        case json_token_type::true_token:
            return ff::value::create<bool>(true);

        case json_token_type::false_token:
            return ff::value::create<bool>(false);

        case json_token_type::null_token:
            return ff::value::create<nullptr_t>();

        case json_token_type::number_token:
            {
                double val;
                std::from_chars_result result = std::from_chars(this->begin(), this->end(), val);
                if (result.ec != std::errc::invalid_argument && result.ptr == this->end())
                {
                    if (std::floor(val) == val && val >= std::numeric_limits<int>::min() && val <= std::numeric_limits<int>::max())
                    {
                        return ff::value::create<int>(static_cast<int>(val));
                    }
                    else
                    {
                        return ff::value::create<double>(val);
                    }
                }
            }
            break;

        case json_token_type::string_token:
            {
                std::string val;
                val.reserve(this->text.size());

                const char* cur = this->text.data() + 1;
                for (const char* end = this->text.data() + this->text.size() - 1; cur && cur < end; )
                {
                    if (*cur == '\\')
                    {
                        switch (cur[1])
                        {
                            case '\"':
                            case '\\':
                            case '/':
                                val.append(1, cur[1]);
                                cur += 2;
                                break;

                            case 'b':
                                val.append(1, '\b');
                                cur += 2;
                                break;

                            case 'f':
                                val.append(1, '\f');
                                cur += 2;
                                break;

                            case 'n':
                                val.append(1, '\n');
                                cur += 2;
                                break;

                            case 'r':
                                val.append(1, '\r');
                                cur += 2;
                                break;

                            case 't':
                                val.append(1, '\t');
                                cur += 2;
                                break;

                            case 'u':
                                if (cur + 6 <= end)
                                {
                                    unsigned int decoded;
                                    std::from_chars_result result = std::from_chars(cur + 2, cur + 6, decoded, 16);
                                    if (result.ec != std::errc::invalid_argument && result.ptr == cur + 6)
                                    {
                                        val.append(1, static_cast<char>(decoded & 0xFF));
                                        cur += 6;
                                    }
                                    else
                                    {
                                        cur = nullptr;
                                    }
                                }
                                else
                                {
                                    cur = nullptr;
                                }
                                break;

                            default:
                                cur = nullptr;
                                break;
                        }
                    }
                    else
                    {
                        val.append(1, *cur);
                        cur++;
                    }
                }

                if (cur)
                {
                    return ff::value::create<std::string>(std::move(val));
                }
            }
            break;
    }

    return nullptr;
}

const char* ff::internal::json_token::begin() const
{
    return this->text.data();
}

const char* ff::internal::json_token::end() const
{
    return this->text.data() + this->text.size();
}

size_t ff::internal::json_token::size() const
{
    return this->text.size();
}

ff::internal::json_tokenizer::json_tokenizer(std::string_view text)
    : pos(text.data())
    , end(text.data() + text.size())
{}

ff::internal::json_token ff::internal::json_tokenizer::next()
{
    char ch = this->skip_spaces_and_comments(this->current_char());
    json_token_type type = json_token_type::error;
    const char* start = this->pos;

    switch (ch)
    {
        case '\0':
            type = json_token_type::none;
            break;

        case 't':
            if (this->skip_identifier(ch) &&
                this->pos - start == 4 &&
                start[1] == 'r' &&
                start[2] == 'u' &&
                start[3] == 'e')
            {
                type = json_token_type::true_token;
            }
            break;

        case 'f':
            if (this->skip_identifier(ch) &&
                this->pos - start == 5 &&
                start[1] == 'a' &&
                start[2] == 'l' &&
                start[3] == 's' &&
                start[4] == 'e')
            {
                type = json_token_type::false_token;
            }
            break;

        case 'n':
            if (this->skip_identifier(ch) &&
                this->pos - start == 4 &&
                start[1] == 'u' &&
                start[2] == 'l' &&
                start[3] == 'l')
            {
                type = json_token_type::null_token;
            }
            break;

        case '\"':
            if (this->skip_string(ch))
            {
                type = json_token_type::string_token;
            }
            break;

        case ',':
            this->pos++;
            type = json_token_type::comma;
            break;

        case ':':
            this->pos++;
            type = json_token_type::colon;
            break;

        case '{':
            this->pos++;
            type = json_token_type::open_curly;
            break;

        case '}':
            this->pos++;
            type = json_token_type::close_curly;
            break;

        case '[':
            this->pos++;
            type = json_token_type::open_bracket;
            break;

        case ']':
            this->pos++;
            type = json_token_type::close_bracket;
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
            if (this->skip_number(ch))
            {
                type = json_token_type::number_token;
            }
            break;
    }

    return json_token{ type, std::string_view(start, static_cast<size_t>(this->pos - start)) };
}

bool ff::internal::json_tokenizer::skip_string(char& ch)
{
    if (ch != '\"')
    {
        return false;
    }

    ch = this->next_char();

    while (true)
    {
        if (ch == '\"')
        {
            this->pos++;
            break;
        }
        else if (ch == '\\')
        {
            ch = this->next_char();

            switch (ch)
            {
                case '\"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    ch = this->next_char();
                    break;

                case 'u':
                    if (this->pos > this->end - 5)
                    {
                        return false;
                    }

                    if (!std::isxdigit(this->pos[1]) ||
                        !std::isxdigit(this->pos[2]) ||
                        !std::isxdigit(this->pos[3]) ||
                        !std::isxdigit(this->pos[4]))
                    {
                        return false;
                    }

                    this->pos += 5;
                    ch = this->current_char();
                    break;

                default:
                    return false;
            }
        }
        else if (ch < ' ')
        {
            return false;
        }
        else
        {
            ch = this->next_char();
        }
    }

    return true;
}

bool ff::internal::json_tokenizer::skip_number(char& ch)
{
    if (ch == '-')
    {
        ch = this->next_char();
    }

    if (!this->skip_digits(ch))
    {
        return false;
    }

    if (ch == '.')
    {
        ch = this->next_char();

        if (!this->skip_digits(ch))
        {
            return false;
        }
    }

    if (ch == 'e' || ch == 'E')
    {
        ch = this->next_char();

        if (ch == '-' || ch == '+')
        {
            ch = this->next_char();
        }

        if (!this->skip_digits(ch))
        {
            return false;
        }
    }

    return true;
}

bool ff::internal::json_tokenizer::skip_digits(char& ch)
{
    if (!std::isdigit(ch))
    {
        return false;
    }

    do
    {
        ch = this->next_char();
    }
    while (std::isdigit(ch));

    return true;
}

bool ff::internal::json_tokenizer::skip_identifier(char& ch)
{
    if (!std::isalpha(ch))
    {
        return false;
    }

    do
    {
        ch = this->next_char();
    }
    while (std::isalnum(ch));

    return true;
}

char ff::internal::json_tokenizer::skip_spaces_and_comments(char ch)
{
    while (true)
    {
        if (std::isspace(ch))
        {
            ch = this->next_char();
        }
        else if (ch == '/')
        {
            char ch2 = this->peek_next_char();

            if (ch2 == '/')
            {
                this->pos++;
                ch = this->next_char();

                while (ch && ch != '\r' && ch != '\n')
                {
                    ch = this->next_char();
                }
            }
            else if (ch2 == '*')
            {
                const char* start = this->pos++;
                ch = this->next_char();

                while (ch && (ch != '*' || this->peek_next_char() != '/'))
                {
                    ch = this->next_char();
                }

                if (!ch)
                {
                    // No end for the comment
                    this->pos = start;
                    ch = '/';
                    break;
                }

                // Skip the end of comment
                this->pos++;
                ch = this->next_char();
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    return ch;
}

char ff::internal::json_tokenizer::current_char() const
{
    return this->pos < this->end ? *this->pos : '\0';
}

char ff::internal::json_tokenizer::next_char()
{
    return ++this->pos < this->end ? *this->pos : '\0';
}

char ff::internal::json_tokenizer::peek_next_char()
{
    return this->pos < this->end - 1 ? this->pos[1] : '\0';
}
