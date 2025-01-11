#pragma once

namespace ff
{
    class init_dx
    {
    public:
        init_dx(bool async = false);
        ~init_dx();

        operator bool() const;

        bool init_async();
        bool init_wait();
    };
}
