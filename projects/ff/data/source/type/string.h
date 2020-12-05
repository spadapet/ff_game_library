#pragma once

#include "../value/value.h"

namespace ff::data::type
{
    class string : public ff::data::value
    {
    public:
    };

    class string_type : public ff::data::value_type_base<string>
    {
    };
}
