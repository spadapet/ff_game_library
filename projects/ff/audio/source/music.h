#pragma once
#include "audio_child_base.h"
#include "audio_effect_base.h"

namespace ff::internal
{
    class music_playing;
}

namespace ff
{
    class music
        : public ff::audio_effect_base
        , public ff::resource_object_base
    {
    public:
        music(const std::shared_ptr<ff::resource>& file_resource, float volume, float speed, bool loop);
        virtual ~music() override;

        virtual void reset() override;
        virtual std::shared_ptr<audio_playing_base> play(bool start_now, float volume, float speed) override;
        virtual bool playing() const override;
        virtual void stop() override;

        std::shared_ptr<ff::internal::music_playing> remove_playing(ff::internal::music_playing* playing);

        virtual std::vector<std::shared_ptr<resource>> resource_get_dependencies() const override;

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        ff::auto_resource<ff::resource_file> file;
        float volume;
        float speed;
        bool loop;

        std::vector<std::shared_ptr<ff::internal::music_playing>> playing_;
    };
}

namespace ff::internal
{
    class music_factory : public ff::resource_object_factory<ff::music>
    {
    public:
        using ff::resource_object_factory<ff::music>::resource_object_factory;

        virtual std::shared_ptr<ff::resource_object_base> load_from_source(const ff::dict& dict, ff::resource_load_context& context) const override;
        virtual std::shared_ptr<ff::resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
