#include "pch.h"

static std::unique_ptr<ff::init_audio> init_audio;
static std::unique_ptr<ff::init_ui> init_ui;

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

            Assert::Fail(ff::string::to_wstring(std::string_view(error_text)).c_str());
            return true;
        });

    ::init_audio = std::make_unique<ff::init_audio>();
    ::init_ui = std::make_unique<ff::init_ui>(ff::init_ui_params{});

    Assert::IsTrue(*::init_audio);
    Assert::IsTrue(*::init_ui);
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_ui.reset();
    ::init_audio.reset();
}
