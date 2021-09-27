#pragma once
#include "audio_playing_base.h"

namespace ff
{
    class music;
}

namespace ff::internal
{
    class music_playing;
    class source_reader_callback;

    class music_playing
        : public ff::audio_playing_base
        , public IXAudio2VoiceCallback
    {
    public:
        music_playing(ff::music* owner);
        virtual ~music_playing() override;

        bool init(std::shared_ptr<ff::resource_file> file, bool start_now, float volume, float speed, bool loop);
        void clear_owner();

        enum class state_t
        {
            invalid,
            init,
            playing,
            paused,
            done,
        };

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

        // IMFSourceReaderCallback look-alike
        HRESULT OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample);
        HRESULT OnFlush(DWORD dwStreamIndex);
        HRESULT OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent);

    private:
        bool async_init();
        void read_sample();
        std::shared_ptr<ff::internal::music_playing> on_music_done();
        void update_source_volume(IXAudio2SourceVoice* source);

        struct buffer_info
        {
            std::vector<uint8_t> buffer;
            LONGLONG start_time;
            LONGLONG duration;
            UINT64 start_samples;
            DWORD stream_index;
            DWORD stream_flags;
        };

        enum class media_state_t
        {
            none,
            reading,
            flushing,
            done,
        };

        mutable std::recursive_mutex mutex;
        ff::music* owner;
        state_t state;
        media_state_t media_state;
        LONGLONG duration_; // in 100-nanosecond units
        LONGLONG desired_position;
        IXAudio2SourceVoice* source;
        ff::timer fade_timer;
        ff::win_handle async_event; // set when there is no async action running
        std::list<buffer_info> buffer_infos;
        std::shared_ptr<ff::resource_file> file;
        Microsoft::WRL::ComPtr<IMFSourceReader> media_reader;
        Microsoft::WRL::ComPtr<ff::internal::source_reader_callback> media_callback;
        float speed;
        float volume_;
        float play_volume;
        float fade_volume;
        float fade_scale;
        bool loop;
        bool start_playing;
    };
}
