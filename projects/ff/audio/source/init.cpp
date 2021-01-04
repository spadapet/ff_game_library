#include "pch.h"
#include "audio.h"
#include "init.h"

ff::init_audio::init_audio()
{
    static bool did_init = false;
    assert(!did_init);
    did_init = true;

    ff::audio::internal::init();
}

ff::init_audio::~init_audio()
{
    ff::audio::internal::destroy();
}
