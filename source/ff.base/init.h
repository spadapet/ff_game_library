#pragma once

namespace ff
{
    class init_base
    {
    public:
        init_base();
        ~init_base();

        operator bool() const;
    };
}
