#include "pch.h"
#include "audio/audio.h"
#include "audio/audio_effect.h"
#include "audio/music.h"
#include "init.h"
#include "input/input.h"
#include "input/input_mapping.h"

namespace
{
    class one_time_init_dx
    {
    public:
        one_time_init_dx()
        {
            const ff::init_window_params window_params{};
            this->init_base.init_main_window(window_params);
            assert_ret(this->init_base);

            // Resource objects
            ff::resource_object_base::register_factory<ff::internal::audio_effect_factory>("effect");
            ff::resource_object_base::register_factory<ff::internal::music_factory>("music");
            ff::resource_object_base::register_factory<ff::internal::input_mapping_factory>("input");

            this->init_audio_status = ff::internal::audio::init();
            this->init_input_status = ff::internal::input::init();

        }

        ~one_time_init_dx()
        {
            ff::internal::input::destroy();
            this->init_input_status = false;

            ff::internal::audio::destroy();
            this->init_audio_status = false;
        }

        bool valid() const
        {
            return this->init_base && this->init_audio_status && this->init_input_status;
        }

    private:
        bool init_audio_status{};
        bool init_input_status{};
        ff::init_base init_base;
    };
}

static int init_dx_refs;
static std::unique_ptr<::one_time_init_dx> init_dx_data;
static std::mutex init_dx_mutex;

ff::init_dx::init_dx()
{
    std::scoped_lock lock(::init_dx_mutex);

    if (::init_dx_refs++ == 0)
    {
        ::init_dx_data = std::make_unique<::one_time_init_dx>();
    }
}

ff::init_dx::~init_dx()
{
    std::scoped_lock lock(::init_dx_mutex);

    if (::init_dx_refs-- == 1)
    {
        ::init_dx_data.reset();
    }
}

ff::init_dx::operator bool() const
{
    return ::init_dx_data && ::init_dx_data->valid();
}
