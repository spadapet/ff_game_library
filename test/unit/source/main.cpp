#include "pch.h"

static std::unique_ptr<ff::init_audio> init_audio;
static std::unique_ptr<ff::init_main_window> init_main_window;
static std::unique_ptr<ff::init_input> init_input;

TEST_MODULE_INITIALIZE(module_init)
{
    ::init_audio = std::make_unique<ff::init_audio>();
    ::init_main_window = std::make_unique<ff::init_main_window>("Unit Tests");
    ::init_input = std::make_unique<ff::init_input>();
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_input.reset();
    ::init_main_window.reset();
    ::init_audio.reset();
}
