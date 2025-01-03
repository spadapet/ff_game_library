#pragma once

#include "../audio/audio_effect_base.h"

namespace ff::internal
{
    class audio_effect_playing;
}

namespace ff
{
    class audio_effect
        : public ff::audio_effect_base
        , public ff::resource_object_base
    {
    public:
        audio_effect(
            const std::shared_ptr<ff::resource>& file_resource,
            size_t start,
            size_t length,
            size_t loop_start,
            size_t loop_length,
            size_t loop_count,
            float volume,
            float speed);
        virtual ~audio_effect() override;

        virtual void reset() override;
        virtual std::shared_ptr<audio_playing_base> play(bool start_now, float volume, float speed) override;
        virtual bool playing() const override;
        virtual void stop() override;

        const WAVEFORMATEX& format() const;
        const std::shared_ptr<ff::data_base> data() const;
        std::shared_ptr<ff::internal::audio_effect_playing> remove_playing(ff::internal::audio_effect_playing* playing);

        virtual bool resource_load_complete(bool from_source) override;
        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict) const override;

    private:
        ff::auto_resource<ff::resource_file> file;
        std::shared_ptr<ff::data_base> data_;
        WAVEFORMATEX format_;
        size_t start;
        size_t length;
        size_t loop_start;
        size_t loop_length;
        size_t loop_count;
        float volume;
        float speed;

        std::vector<std::shared_ptr<ff::internal::audio_effect_playing>> playing_;
    };
}

namespace ff::internal
{
    class audio_effect_factory : public ff::resource_object_factory<ff::audio_effect>
    {
    public:
        using ff::resource_object_factory<ff::audio_effect>::resource_object_factory;

        virtual std::shared_ptr<ff::resource_object_base> load_from_source(const ff::dict& dict, ff::resource_load_context& context) const override;
        virtual std::shared_ptr<ff::resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
