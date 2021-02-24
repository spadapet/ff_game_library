#pragma once

namespace ff
{
    class init_resource
    {
    public:
        init_resource();
        ~init_resource();

        operator bool() const;

    private:
        ff::init_data init_data;
    };
}
