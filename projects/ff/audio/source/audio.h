#pragma once

namespace ff
{
    class audio_playing_base;
}

namespace ff::internal
{
    class audio_child_base;
}

namespace ff::audio
{
    enum class voice_type
    {
        master,
        effects,
        music,
    };

    void stop();
    void start();

    float volume(voice_type type);
    void volume(voice_type type, float volume);

    void advance_effects();
    void stop_effects();
    void pause_effects();
    void resume_effects();
}

namespace ff::audio::internal
{
    bool init();
    void destroy();

    void add_child(ff::internal::audio_child_base* child);
    void remove_child(ff::internal::audio_child_base* child);
    void add_playing(ff::audio_playing_base* child);
    void remove_playing(ff::audio_playing_base* child);

    IXAudio2* xaudio();
    IXAudio2Voice* xaudio_voice(voice_type type);
}
