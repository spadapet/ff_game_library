#pragma once

namespace ff
{
    class init_audio
    {
    public:
        init_audio();
        ~init_audio();

        bool status() const;

    private:
        ff::init_resource init_resource;
    };
}
