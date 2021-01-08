#include "pch.h"
#include "audio.h"
#include "music.h"
#include "music_playing.h"

static ff::pool_allocator<ff::internal::music_playing> music_pool;

static void delete_music(ff::audio_playing_base* value)
{
    ::music_pool.delete_obj(static_cast<ff::internal::music_playing*>(value));
}

ff::music_o::music_o(const std::shared_ptr<ff::resource>& file_resource, float volume, float speed, bool loop)
    : file(file_resource)
    , volume(volume)
    , speed(speed)
    , loop(loop)
{
    ff::audio::internal::add_child(this);
}

ff::music_o::~music_o()
{
    this->stop();

    ff::audio::internal::remove_child(this);
}

void ff::music_o::reset()
{
}

std::shared_ptr<ff::audio_playing_base> ff::music_o::play(bool start_now, float volume, float speed)
{
#if 0
    noAssertRetVal(_device->IsValid() && _streamRes.GetObject(), false);

    ff::ComPtr<AudioMusicPlaying, ff::IAudioPlaying> playing;
    assertHrRetVal(ff::ComAllocator<AudioMusicPlaying>::CreateInstance(_device, &playing), false);
    assertRetVal(playing->Init(this, _streamRes.GetObject(), startPlaying, _volume * volume, _freqRatio * freqRatio, _loop), false);
    _playing.Push(playing);
#endif

    return nullptr;
}

bool ff::music_o::playing() const
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

void ff::music_o::stop()
{
    std::vector<std::shared_ptr<ff::internal::music_playing>> playing;
    std::swap(playing, this->playing_);

    for (const auto& i : playing)
    {
        i->clear_owner();
        i->stop();
    }
}

std::shared_ptr<ff::internal::music_playing> ff::music_o::remove_playing(ff::internal::music_playing* playing)
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

std::vector<std::shared_ptr<ff::resource>> ff::music_o::resource_get_dependencies() const
{
    return std::vector<std::shared_ptr<resource>>
    {
        this->file.resource()
    };
}

bool ff::music_o::save_to_cache(ff::dict& dict, bool& allow_compress) const
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

    return std::make_shared<music_o>(file, volume, speed, loop);
}
