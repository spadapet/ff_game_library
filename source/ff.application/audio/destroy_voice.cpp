#include "pch.h"
#include "audio/audio.h"
#include "audio/audio_child_base.h"
#include "audio/destroy_voice.h"

namespace
{
    class destroy_voice_work : public ff::internal::audio_child_base
    {
    public:
        destroy_voice_work(IXAudio2SourceVoice* source)
            : source(source)
        {
            ff::internal::audio::add_child(this);
        }

        virtual ~destroy_voice_work() override
        {
            this->reset();
            ff::internal::audio::remove_child(this);
        }

        virtual void reset() override
        {
            if (this->source)
            {
                std::scoped_lock lock(this->mutex);
                if (this->source)
                {
                    this->source->DestroyVoice();
                    this->source = nullptr;
                }
            }
        }

    private:
        std::mutex mutex;
        IXAudio2SourceVoice* source;
    };
}

ff::co_task<> ff::internal::destroy_voice_async(IXAudio2SourceVoice* source)
{
    static ff::pool_allocator<destroy_voice_work> pool;

    destroy_voice_work* work = pool.new_obj(source);
    co_await ff::task::resume_on_task();
    pool.delete_obj(work);
}
