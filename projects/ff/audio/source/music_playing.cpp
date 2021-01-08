#include "pch.h"
#include "audio.h"
#include "music.h"
#include "music_playing.h"
#include "destroy_voice.h"

ff::internal::music_playing::music_playing(music_o* owner)
    : owner(owner)
    , source(nullptr)
    , paused_(true)
    , done(false)
{
    ff::audio::internal::add_playing(this);
}

ff::internal::music_playing::~music_playing()
{
    this->reset();

    ff::audio::internal::remove_playing(this);
}

void ff::internal::music_playing::init(IXAudio2SourceVoice* source, bool start_now)
{
    this->source = source;

    if (start_now)
    {
        this->resume();
    }
}

void ff::internal::music_playing::clear_owner()
{
    this->owner = nullptr;
}

void ff::internal::music_playing::reset()
{
    IXAudio2SourceVoice* source = this->source;
    if (source)
    {
        this->source = nullptr;
        source->DestroyVoice();
    }
}

bool ff::internal::music_playing::playing() const
{
    return this->source && !this->paused_ && !this->done;
}

bool ff::internal::music_playing::paused() const
{
    return this->source && this->paused_ && !this->done;
}

bool ff::internal::music_playing::stopped() const
{
    return !this->source || this->done;
}

bool ff::internal::music_playing::music() const
{
    return false;
}

void ff::internal::music_playing::advance()
{
    if (this->done)
    {
        IXAudio2SourceVoice* source = this->source;
        if (source)
        {
            this->source = nullptr;
            ff::internal::destroy_voice_async(source);
        }

        ff::music_o* owner = this->owner;
        if (owner)
        {
            this->owner = nullptr;
            owner->remove_playing(this);
            // don't use "this" here
        }
    }
}

void ff::internal::music_playing::stop()
{
    if (this->source && !this->done)
    {
        this->source->Stop();
        this->source->FlushSourceBuffers();
    }
}

void ff::internal::music_playing::pause()
{
    if (this->source && !this->done)
    {
        this->source->Stop();
        this->paused_ = true;
    }
}

void ff::internal::music_playing::resume()
{
    if (this->paused())
    {
        this->source->Start();
        this->paused_ = false;
    }
}

double ff::internal::music_playing::duration() const
{
    return 0.0;
}

double ff::internal::music_playing::position() const
{
    return 0.0;
}

bool ff::internal::music_playing::position(double value)
{
    return false;
}

double ff::internal::music_playing::volume() const
{
    return 0.0;
}

bool ff::internal::music_playing::volume(double value)
{
    return false;
}

bool ff::internal::music_playing::fade_in(double value)
{
    return false;
}

bool ff::internal::music_playing::fade_out(double value)
{
    return false;
}

void __stdcall ff::internal::music_playing::OnVoiceProcessingPassStart(UINT32 BytesRequired)
{}

void __stdcall ff::internal::music_playing::OnVoiceProcessingPassEnd()
{}

void __stdcall ff::internal::music_playing::OnStreamEnd()
{}

void __stdcall ff::internal::music_playing::OnBufferStart(void* pBufferContext)
{}

void __stdcall ff::internal::music_playing::OnBufferEnd(void* pBufferContext)
{
    this->done = true;
}

void __stdcall ff::internal::music_playing::OnLoopEnd(void* pBufferContext)
{}

void __stdcall ff::internal::music_playing::OnVoiceError(void* pBufferContext, HRESULT error)
{
    assert(false);
}
