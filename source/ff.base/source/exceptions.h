#pragma once

namespace ff
{
    class cancel_exception : public std::exception
    {
    public:
        virtual char const* what() const override;
    };

    class timeout_exception : public std::exception
    {
    public:
        virtual char const* what() const override;
    };
}
