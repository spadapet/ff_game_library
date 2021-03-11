#include "pch.h"

static std::unique_ptr<ff::init_audio> init_audio;
static std::unique_ptr<ff::init_ui> init_ui;

TEST_MODULE_INITIALIZE(module_init)
{
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
