#include "pch.h"

static std::unique_ptr<ff::init_dx> init_dx;

TEST_MODULE_INITIALIZE(module_init)
{
    ff::internal::assert_listener([](const char* exp, const char* text, const char* file, unsigned int line)
    {
        char error_text[1024];

        _snprintf_s(error_text, _countof(error_text), _TRUNCATE,
            "\r\nMessage: %s\r\nExpression: %s\r\nFile: %s (%u)",
            text ? text : "",
            exp ? exp : "",
            file ? file : "",
            line);

        if (ff::constants::debug_build && ::IsDebuggerPresent())
        {
            __debugbreak();
        }

        Assert::Fail(ff::string::to_wstring(std::string_view(error_text)).c_str());
        return true;
    });

    ::init_dx = std::make_unique<ff::init_dx>(ff::init_dx_params{});

    Assert::IsTrue(*::init_dx);
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_dx.reset();
}
