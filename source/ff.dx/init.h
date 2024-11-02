#pragma once

namespace dxgi
{
    class client_functions;
    class host_functions;
}

namespace ff
{
    class init_dx
    {
    public:
        init_dx();
        ~init_dx();

        operator bool() const;
    };
}
