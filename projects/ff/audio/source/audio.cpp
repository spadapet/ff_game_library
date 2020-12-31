#include "pch.h"
#include "audio.h"

static Microsoft::WRL::ComPtr<IXAudio2> xaudio2;

bool ff::audio::internal::init()
{
    ::MFStartup(MF_VERSION);

    if (SUCCEEDED(::XAudio2Create(&::xaudio2)))
    {
#ifdef _DEBUG
        XAUDIO2_DEBUG_CONFIGURATION dc{};
        dc.TraceMask = XAUDIO2_LOG_ERRORS;
        dc.BreakMask = XAUDIO2_LOG_ERRORS;
        ::xaudio2->SetDebugConfiguration(&dc);
#endif

        return true;
    }

    return false;
}

void ff::audio::internal::cleanup()
{
    ::xaudio2.Reset();
    ::MFShutdown();
}
