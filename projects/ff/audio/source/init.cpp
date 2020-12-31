#include "pch.h"
#include "audio.h"
#include "init.h"

static int init_status = 0;

ff::init_audio::init_audio()
{
    assert(!::init_status); // can't init twice
    ::init_status = ff::audio::internal::init() ? 1 : -1;
}

ff::init_audio::~init_audio()
{
    ff::audio::internal::cleanup();
    ::init_status = 0;
}

bool ff::init_audio::status() const
{
    return ::init_status > 0;
}
