#include "pch.h"

static std::unique_ptr<ff::init_audio> init_audio;
static std::unique_ptr<ff::init_graphics> init_graphics;
static std::unique_ptr<ff::init_main_window> init_main_window;
static std::unique_ptr<ff::init_input> init_input;
static std::unique_ptr<ff::init_ui> init_ui;

TEST_MODULE_INITIALIZE(module_init)
{
    ::init_audio = std::make_unique<ff::init_audio>();
    ::init_graphics = std::make_unique<ff::init_graphics>();
    ::init_main_window = std::make_unique<ff::init_main_window>("Unit Tests", false);
    ::init_input = std::make_unique<ff::init_input>();
    ::init_ui = std::make_unique<ff::init_ui>(ff::init_ui_params());

    Assert::IsTrue(*::init_audio);
    Assert::IsTrue(*::init_graphics);
    Assert::IsTrue(*::init_main_window);
    Assert::IsTrue(*::init_input);
    Assert::IsTrue(*::init_ui);
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_ui.reset();
    ::init_input.reset();
    ::init_main_window.reset();
    ::init_graphics.reset();
    ::init_audio.reset();
}
