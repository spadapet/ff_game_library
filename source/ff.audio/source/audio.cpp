#include "pch.h"
#include "audio.h"
#include "audio_child_base.h"
#include "audio_playing_base.h"

static Microsoft::WRL::ComPtr<IXAudio2> xaudio2;
static IXAudio2MasteringVoice* master_voice = nullptr;
static IXAudio2SubmixVoice* effect_voice = nullptr;
static IXAudio2SubmixVoice* music_voice = nullptr;

static std::recursive_mutex audio_mutex;
static std::vector<ff::internal::audio_child_base*> audio_children;
static std::vector<ff::audio_playing_base*> audio_playing;
static std::vector<ff::audio_playing_base*> audio_paused;

template<class T>
static void get_copy(ff::stack_vector<T*, 64>& dest, const std::vector<T*>& src)
{
    std::scoped_lock lock(::audio_mutex);
    dest.resize(src.size());

    if (src.size())
    {
        std::memcpy(dest.data(), src.data(), ff::vector_byte_size(src));
    }
}

static bool init_mastering_voice()
{
    if (::xaudio2 && !::master_voice && SUCCEEDED(::xaudio2->CreateMasteringVoice(&::master_voice)))
    {
        XAUDIO2_SEND_DESCRIPTOR master_descriptor{};
        master_descriptor.pOutputVoice = ::master_voice;

        XAUDIO2_VOICE_SENDS master_send{};
        master_send.SendCount = 1;
        master_send.pSends = &master_descriptor;

        XAUDIO2_VOICE_DETAILS details{};
        ::master_voice->GetVoiceDetails(&details);

        if (FAILED(::xaudio2->CreateSubmixVoice(&::effect_voice, details.InputChannels, details.InputSampleRate, 0, 0, &master_send)) ||
            FAILED(::xaudio2->CreateSubmixVoice(&::music_voice, details.InputChannels, details.InputSampleRate, 0, 0, &master_send)))
        {
            assert(false);
            return false;
        }
    }

    return true;
}

static void destroy_mastering_voice()
{
    ff::stack_vector<ff::internal::audio_child_base*, 64> audio_children_copy;
    ::get_copy(audio_children_copy, ::audio_children);

    for (ff::internal::audio_child_base* child : audio_children_copy)
    {
        child->reset();
    }

    ff::audio::stop();

    if (::effect_voice)
    {
        ::effect_voice->DestroyVoice();
        ::effect_voice = nullptr;
    }

    if (::music_voice)
    {
        ::music_voice->DestroyVoice();
        ::music_voice = nullptr;
    }

    if (::master_voice)
    {
        ::master_voice->DestroyVoice();
        ::master_voice = nullptr;
    }
}

bool ff::internal::audio::init()
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
}

    return ::init_mastering_voice();
}

void ff::internal::audio::destroy()
{
    ::destroy_mastering_voice();
    ::xaudio2.Reset();
    ::MFShutdown();
}

void ff::internal::audio::add_child(ff::internal::audio_child_base* child)
{
    std::scoped_lock lock(::audio_mutex);
    ::audio_children.push_back(child);
}

void ff::internal::audio::remove_child(ff::internal::audio_child_base* child)
{
    std::scoped_lock lock(::audio_mutex);
    auto i = std::find(::audio_children.cbegin(), ::audio_children.cend(), child);
    if (i != ::audio_children.cend())
    {
        ::audio_children.erase(i);
    }
}

void ff::internal::audio::add_playing(ff::audio_playing_base* child)
{
    ff::internal::audio::add_child(child);

    std::scoped_lock lock(::audio_mutex);
    ::audio_playing.push_back(child);

    if (child->paused())
    {
        ::audio_paused.push_back(child);
    }
}

void ff::internal::audio::remove_playing(ff::audio_playing_base* child)
{
    ff::internal::audio::remove_child(child);

    std::scoped_lock lock(::audio_mutex);

    auto i = std::find(::audio_paused.cbegin(), ::audio_paused.cend(), child);
    if (i != ::audio_paused.cend())
    {
        ::audio_paused.erase(i);
    }

    i = std::find(::audio_playing.cbegin(), ::audio_playing.cend(), child);
    if (i != ::audio_playing.cend())
    {
        ::audio_playing.erase(i);
    }
}

IXAudio2* ff::internal::audio::xaudio()
{
    return ::xaudio2.Get();
}

IXAudio2Voice* ff::internal::audio::xaudio_voice(ff::audio::voice_type type)
{
    switch (type)
    {
        case ff::audio::voice_type::effects:
            return ::effect_voice;

        case ff::audio::voice_type::music:
            return ::music_voice;

        default:
        case ff::audio::voice_type::master:
            return ::master_voice;
    }
}

void ff::audio::stop()
{
    ff::audio::stop_effects();

    if (::xaudio2)
    {
        ::xaudio2->StopEngine();
    }
}

void ff::audio::start()
{
    if (::xaudio2)
    {
        ::xaudio2->StartEngine();
    }
}

float ff::audio::volume(voice_type type)
{
    float volume = 1;

    IXAudio2Voice* voice = ff::internal::audio::xaudio_voice(type);
    if (voice)
    {
        voice->GetVolume(&volume);
    }

    return volume;
}

void ff::audio::volume(voice_type type, float volume)
{
    IXAudio2Voice* voice = ff::internal::audio::xaudio_voice(type);
    if (voice)
    {
        volume = std::max<float>(0, volume);
        volume = std::min<float>(1, volume);

        voice->SetVolume(volume);
    }
}

void ff::audio::advance_effects()
{
    static size_t advances = 0;

    // Check if speakers were plugged in every two seconds
    if (!::master_voice && ++advances % 120 == 0)
    {
        ::init_mastering_voice();
    }

    ff::stack_vector<ff::audio_playing_base*, 64> audio_playing_copy;
    ::get_copy(audio_playing_copy, ::audio_playing);

    for (ff::audio_playing_base* playing : audio_playing_copy)
    {
        playing->advance();
    }
}

void ff::audio::stop_effects()
{
    ff::stack_vector<ff::audio_playing_base*, 64> audio_playing_copy;
    ::get_copy(audio_playing_copy, ::audio_playing);

    for (ff::audio_playing_base* playing : audio_playing_copy)
    {
        playing->stop();
    }
}

void ff::audio::pause_effects()
{
    ff::stack_vector<ff::audio_playing_base*, 64> new_paused;
    ff::stack_vector<ff::audio_playing_base*, 64> audio_playing_copy;
    ::get_copy(audio_playing_copy, ::audio_playing);

    for (ff::audio_playing_base* playing : audio_playing_copy)
    {
        playing->pause();

        if (playing->paused())
        {
            new_paused.push_back(playing);
        }
    }

    std::scoped_lock lock(::audio_mutex);

    for (ff::audio_playing_base* paused : new_paused)
    {
        if (std::find(::audio_paused.cbegin(), ::audio_paused.cend(), paused) == ::audio_paused.cend())
        {
            ::audio_paused.push_back(paused);
        }
    }
}

void ff::audio::resume_effects()
{
    std::vector<ff::audio_playing_base*> paused;
    {
        std::scoped_lock lock(::audio_mutex);
        std::swap(paused, ::audio_paused);
    }

    for (ff::audio_playing_base* paused : paused)
    {
        paused->resume();
    }
}
