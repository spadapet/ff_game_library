#include "pch.h"

static std::unique_ptr<ff::init_audio> init_audio;

TEST_MODULE_INITIALIZE(module_init)
{
    ::init_audio = std::make_unique<ff::init_audio>();
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_audio.reset();
}
