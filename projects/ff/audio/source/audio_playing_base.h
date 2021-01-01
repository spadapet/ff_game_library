#pragma once

namespace ff::audio
{
    class audio_playing_base
    {
    public:
        virtual ~audio_playing_base() = 0;

        virtual bool playing() const = 0;
        virtual bool paused() const = 0;
        virtual bool stopped() const = 0;
        virtual bool music() const = 0;

        virtual void advance() = 0;
        virtual void stop() = 0;
        virtual void pause() = 0;
        virtual void resume() = 0;

        virtual double duration() const = 0; // in seconds
        virtual double position() const = 0; // in seconds
        virtual bool position(double value) = 0; // in seconds
        virtual double volume() const = 0;
        virtual bool volume(double value) = 0;
        virtual bool fade_in(double value) = 0;
        virtual bool fade_out(double value) = 0;
    };
}
