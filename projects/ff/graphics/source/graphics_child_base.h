#pragma once

namespace ff::internal
{
    class graphics_child_base
    {
    public:
        virtual ~graphics_child_base() = 0;

        virtual bool reset() = 0;
        virtual int reset_pritory() const;
    };
}