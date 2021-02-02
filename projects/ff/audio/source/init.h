#pragma once

namespace ff
{
    class init_audio
    {
    public:
        init_audio();
        ~init_audio();

        operator bool() const;

    private:
        ff::init_resource init_resource;
    };
}
