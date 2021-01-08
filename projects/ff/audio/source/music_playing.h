#pragma once
#include "audio_playing_base.h"

namespace ff
{
    class music_o;
}

namespace ff::internal
{
    class music_playing
        : public ff::audio_playing_base
        , public IXAudio2VoiceCallback
    {
    public:
        music_playing(music_o* owner);
        virtual ~music_playing() override;

        void init(IXAudio2SourceVoice* source, bool start_now);
        void clear_owner();

        // audio_child_base
        virtual void reset() override;

        // audio_playing_base
        virtual bool playing() const override;
        virtual bool paused() const override;
        virtual bool stopped() const override;
        virtual bool music() const override;

        virtual void advance() override;
        virtual void stop() override;
        virtual void pause() override;
        virtual void resume() override;

        virtual double duration() const override;
        virtual double position() const override;
        virtual bool position(double value) override;
        virtual double volume() const override;
        virtual bool volume(double value) override;
        virtual bool fade_in(double value) override;
        virtual bool fade_out(double value) override;

        // IXAudio2VoiceCallback
        virtual void __stdcall OnVoiceProcessingPassStart(UINT32 BytesRequired) override;
        virtual void __stdcall OnVoiceProcessingPassEnd() override;
        virtual void __stdcall OnStreamEnd() override;
        virtual void __stdcall OnBufferStart(void* pBufferContext) override;
        virtual void __stdcall OnBufferEnd(void* pBufferContext) override;
        virtual void __stdcall OnLoopEnd(void* pBufferContext) override;
        virtual void __stdcall OnVoiceError(void* pBufferContext, HRESULT error) override;

    private:
        music_o* owner;
        std::shared_ptr<ff::data_base> data;
        IXAudio2SourceVoice* source;
        bool paused_;
        bool done;
    };
}
