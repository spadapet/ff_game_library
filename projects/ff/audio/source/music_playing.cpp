#include "pch.h"
#include "audio.h"
#include "music.h"
#include "music_playing.h"
#include "destroy_voice.h"

static const size_t MAX_BUFFERS = 2;

namespace ff::internal
{
    class source_reader_callback : public IMFSourceReaderCallback
    {
    public:
        source_reader_callback(music_playing* owner)
            : owner(owner)
            , refs(0)
        {}

        virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (!ppvObject)
            {
                return E_POINTER;
            }
            else if (riid == __uuidof(IUnknown))
            {
                *ppvObject = static_cast<IUnknown*>(this);
            }
            else if (riid == __uuidof(IMFSourceReaderCallback))
            {
                *ppvObject = static_cast<IMFSourceReaderCallback*>(this);
            }
            else
            {
                return E_NOINTERFACE;
            }

            this->AddRef();
            return S_OK;
        }

        virtual ULONG __stdcall AddRef() override
        {
            return this->refs.fetch_add(1) + 1;
        }

        virtual ULONG __stdcall Release() override
        {
            ULONG refs = this->refs.fetch_sub(1) - 1;
            if (!refs)
            {
                delete this;
            }

            return refs;
        }

        void clear_owner()
        {
            std::lock_guard lock(this->mutex);
            this->owner = nullptr;
        }

        virtual HRESULT __stdcall OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample) override
        {
            std::lock_guard lock(this->mutex);
            return this->owner ? this->owner->OnReadSample(hrStatus, dwStreamIndex, dwStreamFlags, llTimestamp, pSample) : S_OK;
        }

        virtual HRESULT __stdcall OnFlush(DWORD dwStreamIndex) override
        {
            std::lock_guard lock(this->mutex);
            return this->owner ? this->owner->OnFlush(dwStreamIndex) : S_OK;
        }

        virtual HRESULT __stdcall OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent) override
        {
            std::lock_guard lock(this->mutex);
            return this->owner ? this->owner->OnEvent(dwStreamIndex, pEvent) : S_OK;
        }

    private:
        std::recursive_mutex mutex;
        std::atomic_ulong refs;
        music_playing* owner;
    };
}

ff::internal::music_playing::music_playing(music_o* owner)
    : owner(owner)
    , state(state_t::invalid)
    , media_state(media_state_t::none)
    , duration_(0)
    , desired_position(0)
    , source(nullptr)
    , async_event(ff::create_event(true))
    , stop_event(ff::create_event())
    , media_callback(new source_reader_callback(this))
    , speed(1)
    , volume_(1)
    , play_volume(1)
    , fade_volume(1)
    , fade_scale(0)
    , loop(false)
    , start_playing(false)
{
    ff::audio::internal::add_playing(this);
}

ff::internal::music_playing::~music_playing()
{
    this->reset();
    this->media_callback->clear_owner();

    ff::audio::internal::remove_playing(this);
}

void ff::internal::music_playing::init(std::shared_ptr<ff::file_o> file, bool start_now, float volume, float speed, bool loop)
{
    assert(this->state == state_t::invalid);
    if (ff::audio::internal::xaudio() && ff::audio::internal::xaudio_voice(ff::audio::voice_type::music))
    {
        this->state = state_t::init;
        this->file = file;
        this->start_playing = start_playing;
        this->volume_ = volume;
        this->speed = speed;
        this->loop = loop;

        this->async_start();
    }
}

void ff::internal::music_playing::clear_owner()
{
    std::lock_guard lock(this->mutex);
    this->owner = nullptr;
}

void ff::internal::music_playing::reset()
{
    this->async_cancel();

    std::lock_guard lock(this->mutex);

    if (this->source)
    {
        IXAudio2SourceVoice* source = this->source;
        this->source = nullptr;
        source->DestroyVoice();
    }

    this->state = state_t::done;
}

bool ff::internal::music_playing::playing() const
{
    std::lock_guard lock(this->mutex);
    return this->state == state_t::playing || (this->state == state_t::init && this->start_playing);
}

bool ff::internal::music_playing::paused() const
{
    std::lock_guard lock(this->mutex);
    return this->state == state_t::paused || (this->state == state_t::init && !this->start_playing);
}

bool ff::internal::music_playing::stopped() const
{
    std::lock_guard lock(this->mutex);
    return this->state == state_t::done;
}

bool ff::internal::music_playing::music() const
{
    return true;
}

void ff::internal::music_playing::advance()
{
    if (this->state == state_t::playing && this->source && this->fade_scale != 0)
    {
        // Fade the volume in or out
        this->fade_timer.tick();

        float abs_fade_scale = std::abs(this->fade_scale);
        this->fade_volume = ff::math::clamp(static_cast<float>(this->fade_timer.get_seconds() * abs_fade_scale), 0.0f, 1.0f);

        bool fade_done = (this->fade_volume >= 1);
        bool fade_out = (this->fade_scale < 0);

        if (this->fade_scale < 0)
        {
            this->fade_volume = 1.0f - this->fade_volume;
        }

        this->update_source_volume(this->source);

        if (fade_done)
        {
            this->fade_scale = 0;

            if (fade_out)
            {
                this->stop();
            }
        }
    }

    if (this->state == state_t::done)
    {
        std::shared_ptr<ff::internal::music_playing> keep_alive;
        std::lock_guard lock(this->mutex);

        if (this->state == state_t::done)
        {
            keep_alive = this->on_music_done();
        }
    }
}

void ff::internal::music_playing::stop()
{
    std::lock_guard lock(this->mutex);

    if (this->state != state_t::done)
    {
        if (this->source)
        {
            this->source->Stop();
        }

        this->state = state_t::done;
    }
}

void ff::internal::music_playing::pause()
{
    std::lock_guard lock(this->mutex);

    if (this->state == state_t::init)
    {
        this->start_playing = false;
    }
    else if (this->state == state_t::playing && this->source)
    {
        this->desired_position = static_cast<LONGLONG>(this->position() * 10000000.0);
        this->source->Stop();
        this->state = state_t::paused;
    }
}

void ff::internal::music_playing::resume()
{
    std::lock_guard lock(this->mutex);

    if (this->state == state_t::init)
    {
        this->start_playing = true;
    }
    else if (this->state == state_t::paused && this->source)
    {
        this->source->Start();
        this->state = state_t::playing;
    }
}

double ff::internal::music_playing::duration() const
{
    std::lock_guard lock(this->mutex);

    return this->duration_ / 10000000.0;
}

double ff::internal::music_playing::position() const
{
    std::lock_guard lock(this->mutex);
    double pos = 0;
    bool use_desired_position = false;

    switch (this->state)
    {
        case state_t::init:
        case state_t::paused:
            use_desired_position = true;
            break;

        case state_t::playing:
            {
                XAUDIO2_VOICE_STATE state;
                this->source->GetState(&state);

                XAUDIO2_VOICE_DETAILS details;
                this->source->GetVoiceDetails(&details);

                buffer_info* info = reinterpret_cast<buffer_info*>(state.pCurrentBufferContext);
                if (info && info->start_samples != static_cast<UINT64>(-1))
                {
                    double sampleSeconds = (state.SamplesPlayed > info->start_samples)
                        ? (state.SamplesPlayed - info->start_samples) / static_cast<double>(details.InputSampleRate)
                        : 0.0;
                    pos = sampleSeconds + (info->start_time / 10000000.0);
                }
                else
                {
                    use_desired_position = true;
                }
            }
            break;
    }

    if (use_desired_position)
    {
        pos = this->desired_position / 10000000.0;
    }

    return pos;
}

bool ff::internal::music_playing::position(double value)
{
    std::lock_guard lock(this->mutex);

    if (this->state == state_t::done)
    {
        return false;
    }

    this->desired_position = static_cast<LONGLONG>(value * 10000000.0);

    if (this->media_reader && this->media_state != media_state_t::flushing)
    {
        this->media_state = media_state_t::flushing;

        if (FAILED(this->media_reader->Flush(MF_SOURCE_READER_FIRST_AUDIO_STREAM)))
        {
            this->media_state = media_state_t::none;
            return false;
        }
    }

    return true;
}

double ff::internal::music_playing::volume() const
{
    return this->play_volume;
}

bool ff::internal::music_playing::volume(double value)
{
    this->play_volume = ff::math::clamp(static_cast<float>(value), 0.0f, 1.0f);
    this->update_source_volume(this->source);
    return true;
}

bool ff::internal::music_playing::fade_in(double value)
{
    if (this->playing() || value <= 0)
    {
        return false;
    }

    this->fade_timer.reset();
    this->fade_scale = static_cast<float>(1.0 / ff::math::clamp(value, 0.0, 10.0));
    this->fade_volume = 0;

    this->update_source_volume(this->source);

    return true;
}

bool ff::internal::music_playing::fade_out(double value)
{
    if (!this->playing() || value <= 0)
    {
        return false;
    }

    this->fade_timer.reset();
    this->fade_scale = static_cast<float>(-1.0 / ff::math::clamp(value, 0.0, 10.0));
    this->fade_volume = 1;

    this->update_source_volume(this->source);

    return true;
}

void ff::internal::music_playing::OnVoiceProcessingPassStart(UINT32 BytesRequired)
{}

void ff::internal::music_playing::OnVoiceProcessingPassEnd()
{}

void ff::internal::music_playing::OnStreamEnd()
{
    std::lock_guard lock(this->mutex);
    this->state = state_t::done;
}

void ff::internal::music_playing::OnBufferStart(void* pBufferContext)
{
    std::lock_guard lock(this->mutex);

    if (this->buffer_infos.size() == ::MAX_BUFFERS)
    {
        // Reuse buffers
        assert(&this->buffer_infos.back() == pBufferContext);
        this->buffer_infos.splice(this->buffer_infos.cend(), this->buffer_infos, this->buffer_infos.cbegin());
    }

    if (!this->buffer_infos.empty())
    {
        XAUDIO2_VOICE_STATE state;
        this->source->GetState(&state);

        buffer_info* info = &this->buffer_infos.front();
        assert(state.pCurrentBufferContext == info);
        info->start_samples = state.SamplesPlayed;
    }

    this->read_sample();
}

void ff::internal::music_playing::OnBufferEnd(void* pBufferContext)
{}

void ff::internal::music_playing::OnLoopEnd(void* pBufferContext)
{}

void ff::internal::music_playing::OnVoiceError(void* pBufferContext, HRESULT error)
{
    assert(false);
}

HRESULT ff::internal::music_playing::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample* pSample)
{
    std::lock_guard lock(this->mutex);

    if (this->media_state != media_state_t::reading)
    {
        return S_OK;
    }

    bool end_of_stream = (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;
    this->media_state = end_of_stream ? media_state_t::done : media_state_t::none;

    if (this->source && this->state != state_t::done)
    {
        Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
        buffer_info* info = nullptr;
        BYTE* data = nullptr;
        DWORD data_size = 0;

        if (pSample && SUCCEEDED(hrStatus) &&
            SUCCEEDED(pSample->ConvertToContiguousBuffer(&media_buffer)) &&
            SUCCEEDED(media_buffer->Lock(&data, nullptr, &data_size)))
        {
            if (this->buffer_infos.size() < ::MAX_BUFFERS)
            {
                this->buffer_infos.push_back(buffer_info{});
            }

            info = &this->buffer_infos.back();
            info->buffer.resize(data_size);
            info->start_time = llTimestamp;
            info->start_samples = static_cast<UINT64>(-1);
            info->stream_index = dwStreamIndex;
            info->stream_flags = dwStreamFlags;

            if (FAILED(pSample->GetSampleDuration(&info->duration)))
            {
                info->duration = 0;
            }

            std::memcpy(info->buffer.data(), data, data_size);

            media_buffer->Unlock();
        }

        if (info)
        {
            XAUDIO2_BUFFER buffer{};
            buffer.AudioBytes = static_cast<UINT>(info->buffer.size());
            buffer.pAudioData = info->buffer.data();
            buffer.pContext = info;
            buffer.Flags = end_of_stream ? XAUDIO2_END_OF_STREAM : 0;
            this->source->SubmitSourceBuffer(&buffer);

            if (this->state == state_t::init)
            {
                this->state = state_t::paused;

                if (this->start_playing)
                {
                    this->start_playing = false;
                    this->source->Start();
                    this->state = state_t::playing;
                }
            }
        }
        else if (this->state == state_t::init)
        {
            this->state = state_t::done;
        }
        else
        {
            this->source->Discontinuity();
        }
    }

    return S_OK;
}

HRESULT ff::internal::music_playing::OnFlush(DWORD dwStreamIndex)
{
    std::lock_guard lock(this->mutex);

    assert(this->media_state == media_state_t::flushing);
    this->media_state = media_state_t::none;

    if (this->desired_position >= 0 && this->desired_position <= this->duration_)
    {
        if (this->source)
        {
            this->start_playing = this->start_playing || (this->state == state_t::playing);
            this->source->Stop();
            this->source->FlushSourceBuffers();
            this->state = state_t::init;
        }

        PROPVARIANT value;
        ::PropVariantInit(&value);
        value.vt = VT_I8;
        value.hVal.QuadPart = this->desired_position;

        this->media_reader->SetCurrentPosition(GUID_NULL, value);

        ::PropVariantClear(&value);
    }

    this->read_sample();

    return S_OK;
}

HRESULT ff::internal::music_playing::OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent)
{
    return S_OK;
}

bool ff::internal::music_playing::async_init()
{
    assertRetVal(_stream && _mediaCallback && !this->source && !this->media_reader, false);

    Microsoft::WRL::ComPtr<ff::IDataReader> reader;
    assertRetVal(_stream->CreateReader(&reader), false);

    Microsoft::WRL::ComPtr<IMFByteStream> mediaByteStream;
    assertRetVal(ff::CreateReadStream(reader, _stream->GetMimeType(), &mediaByteStream), false);

    Microsoft::WRL::ComPtr<IMFSourceResolver> sourceResolver;
    assertHrRetVal(::MFCreateSourceResolver(&sourceResolver), false);

    Microsoft::WRL::ComPtr<IUnknown> mediaSourceUnknown;
    MF_OBJECT_TYPE mediaSourceObjectType = MF_OBJECT_MEDIASOURCE;
    DWORD mediaSourceFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ | MF_RESOLUTION_DISABLE_LOCAL_PLUGINS;
    assertHrRetVal(sourceResolver->CreateObjectFromByteStream(mediaByteStream, nullptr, mediaSourceFlags, nullptr, &mediaSourceObjectType, &mediaSourceUnknown), false);

    Microsoft::WRL::ComPtr<IMFMediaSource> mediaSource;
    assertRetVal(mediaSource.QueryFrom(mediaSourceUnknown), false);

    Microsoft::WRL::ComPtr<IMFAttributes> mediaAttributes;
    assertHrRetVal(::MFCreateAttributes(&mediaAttributes, 1), false);
    assertHrRetVal(mediaAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, _mediaCallback.Interface()), false);

    Microsoft::WRL::ComPtr<IMFSourceReader> mediaReader;
    assertHrRetVal(::MFCreateSourceReaderFromMediaSource(mediaSource, mediaAttributes, &mediaReader), false);
    assertHrRetVal(mediaReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false), false);
    assertHrRetVal(mediaReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, true), false);

    Microsoft::WRL::ComPtr<IMFMediaType> mediaType;
    assertHrRetVal(::MFCreateMediaType(&mediaType), false);
    assertHrRetVal(mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), false);
    assertHrRetVal(mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float), false);
    assertHrRetVal(mediaReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaType), false);

    Microsoft::WRL::ComPtr<IMFMediaType> actualMediaType;
    assertHrRetVal(mediaReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actualMediaType), false);

    WAVEFORMATEX* waveFormat = nullptr;
    UINT waveFormatLength = 0;
    assertHrRetVal(::MFCreateWaveFormatExFromMFMediaType(actualMediaType, &waveFormat, &waveFormatLength), false);

    PROPVARIANT durationValue;
    assertHrRetVal(mediaReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durationValue), false);

    XAUDIO2_SEND_DESCRIPTOR sendDesc{ 0 };
    sendDesc.pOutputVoice = _device->AsXAudioDevice()->GetVoice(ff::AudioVoiceType::MUSIC);

    XAUDIO2_VOICE_SENDS sends;
    sends.SendCount = 1;
    sends.pSends = &sendDesc;

    IXAudio2SourceVoice* source = nullptr;
    HRESULT hr = _device->AsXAudioDevice()->GetAudio()->CreateSourceVoice(
        &source,
        waveFormat,
        0, // flags
        XAUDIO2_DEFAULT_FREQ_RATIO,
        this, // callback,
        &sends);

    ::CoTaskMemFree(waveFormat);
    waveFormat = nullptr;

    if (FAILED(hr))
    {
        if (source)
        {
            source->DestroyVoice();
        }

        assertHrRetVal(hr, false);
    }

    std::lock_guard lock(this->mutex);

    this->duration_ = (LONGLONG)durationValue.uhVal.QuadPart;

    if (this->desired_position >= 0 && this->desired_position <= this->duration_)
    {
        PROPVARIANT value;
        ::PropVariantInit(&value);

        value.vt = VT_I8;
        value.hVal.QuadPart = this->desired_position;
        verifyHr(mediaReader->SetCurrentPosition(GUID_NULL, value));

        ::PropVariantClear(&value);
    }

    if (this->state == state_t::MUSIC_INIT)
    {
        UpdateSourceVolume(source);
        source->SetFrequencyRatio(_freqRatio);
        this->source = source;
        this->media_reader = mediaReader;
    }
    else
    {
        source->DestroyVoice();
    }

    assertRetVal(this->source, false);
    return true;
}

void ff::internal::music_playing::async_start()
{
    ::ResetEvent(this->async_event);

    ff::thread_pool::get()->add_task([this]()
        {
            this->async_run();
        });
}

void ff::internal::music_playing::async_run()
{
    if (!this->source && !ff::is_event_set(this->stop_event))
    {
        this->async_init();
    }

    this->read_sample();

    ::SetEvent(this->async_event);
}

void ff::internal::music_playing::async_cancel()
{
    ::SetEvent(this->stop_event);
    ff::wait_for_handle(this->async_event);
    ::ResetEvent(this->stop_event);
}

void ff::internal::music_playing::read_sample()
{
    std::lock_guard lock(this->mutex);
    HRESULT hr = E_FAIL;

    if (this->media_state == media_state_t::none)
    {
        if (this->media_reader)
        {
            this->media_state = media_state_t::reading;

            hr = this->media_reader->ReadSample(
                MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                0, nullptr, nullptr, nullptr, nullptr);
        }

        if (FAILED(hr) && this->source)
        {
            this->source->Discontinuity();
        }
    }
}

std::shared_ptr<ff::internal::music_playing> ff::internal::music_playing::on_music_done()
{
    std::shared_ptr<music_playing> keep_alive;
    std::lock_guard lock(this->mutex);

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
        keep_alive = owner->remove_playing(this);
    }

    return keep_alive;
}

void ff::internal::music_playing::update_source_volume(IXAudio2SourceVoice* source)
{
    if (source)
    {
        source->SetVolume(this->volume_ * this->play_volume * this->fade_volume);
    }
}
