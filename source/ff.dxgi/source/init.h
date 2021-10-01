#pragma once

namespace ff::dxgi
{
    class init
    {
    public:
        init();
        ~init();

        operator bool() const;
    };
}
