#include "pch.h"

static std::unique_ptr<ff::init_graphics> init_graphics;

TEST_MODULE_INITIALIZE(module_init)
{
    ::init_graphics = std::make_unique<ff::init_graphics>();

    Assert::IsTrue(*::init_graphics);
}

TEST_MODULE_CLEANUP(module_cleanup)
{
    ::init_graphics.reset();
}
