#include "pch.h"
#include "audio.h"
#include "music.h"
#include "music_playing.h"

ff::music::music(const std::shared_ptr<ff::resource>& file_resource, float volume, float speed, bool loop)
    : file(file_resource)
    , volume(volume)
    , speed(speed)
    , loop(loop)
{
    ff::internal::audio::add_child(this);
}

ff::music::~music()
{
    this->stop();

    ff::internal::audio::remove_child(this);
}

void ff::music::reset()
{
}

std::shared_ptr<ff::audio_playing_base> ff::music::play(bool start_now, float volume, float speed)
{
    std::shared_ptr<ff::internal::music_playing> playing = std::make_shared<ff::internal::music_playing>(this);
    if (playing->init(this->file.object(), start_now, this->volume * volume, this->speed * speed, this->loop))
    {
        this->playing_.push_back(playing);
        return playing;
    }

    return nullptr;
}

bool ff::music::playing() const
{
    for (const auto& i : this->playing_)
    {
        if (i->playing())
        {
            return true;
        }
    }

    return false;
}

void ff::music::stop()
{
    std::vector<std::shared_ptr<ff::internal::music_playing>> playing;
    std::swap(playing, this->playing_);

    for (const auto& i : playing)
    {
        i->clear_owner();
        i->stop();
    }
}

std::shared_ptr<ff::internal::music_playing> ff::music::remove_playing(ff::internal::music_playing* playing)
{
    for (auto i = this->playing_.cbegin(); i != this->playing_.cend(); ++i)
    {
        if ((*i).get() == playing)
        {
            std::shared_ptr<ff::internal::music_playing> ret = *i;
            this->playing_.erase(i);
            return ret;
        }
    }

    assert(false);
    return nullptr;
}

std::vector<std::shared_ptr<ff::resource>> ff::music::resource_get_dependencies() const
{
    return std::vector<std::shared_ptr<resource>>
    {
        this->file.resource()
    };
}

bool ff::music::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    dict.set<ff::resource>("file", this->file.resource());
    dict.set<float>("volume", this->volume);
    dict.set<float>("speed", this->speed);
    dict.set<bool>("loop", this->loop);

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::music_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return music_factory::load_from_cache(dict);
}

std::shared_ptr<ff::resource_object_base> ff::internal::music_factory::load_from_cache(const ff::dict& dict) const
{
    std::shared_ptr<ff::resource> file = dict.get<ff::resource>("file");
    float volume = dict.get<float>("volume", 1.0f);
    float speed = dict.get<float>("speed", 1.0f);
    bool loop = dict.get<float>("loop");

    return std::make_shared<music>(file, volume, speed, loop);
}
