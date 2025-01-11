#include "pch.h"
#include "audio/wav_file.h"

constexpr DWORD make_four_cc(char ch0, char ch1, char ch2, char ch3)
{
    return
        static_cast<DWORD>(ch0) |
        (static_cast<DWORD>(ch1) << 8) |
        (static_cast<DWORD>(ch2) << 16) |
        (static_cast<DWORD>(ch3) << 24);
}

namespace
{
    class riff_reader
    {
    public:
        bool read(ff::reader_base& reader)
        {
            DWORD id;
            if (!ff::load(reader, id) || id != ::make_four_cc('R', 'I', 'F', 'F'))
            {
                assert(false); // not a RIFF file?
                return false;
            }

            DWORD size;
            if (!ff::load(reader, size) || size < sizeof(DWORD) || !ff::load(reader, id))
            {
                return false;
            }

            size -= sizeof(DWORD);
            std::shared_ptr<ff::saved_data_base> saved_data = reader.saved_data(reader.pos(), size, size, ff::saved_data_type::none);
            if (!saved_data || !this->handle_data(id, saved_data))
            {
                return false;
            }

            return this->internal_read(reader);
        }

    protected:
        virtual bool handle_data(DWORD id, const std::shared_ptr<ff::saved_data_base>& saved_data) = 0;

    private:
        bool internal_read(ff::reader_base& reader)
        {
            while (reader.pos() < reader.size())
            {
                DWORD id, size;
                if (!ff::load(reader, id) || !ff::load(reader, size))
                {
                    return false;
                }

                std::shared_ptr<ff::saved_data_base> saved_data = reader.saved_data(reader.pos(), size, size, ff::saved_data_type::none);
                if (!saved_data || !reader.pos(reader.pos() + size) || !this->handle_data(id, saved_data))
                {
                    return false;
                }
            }

            return true;
        }
    };

    class wav_reader : public riff_reader
    {
    public:
        WAVEFORMATEX format{};
        std::shared_ptr<ff::saved_data_base> wav_data;

    protected:
        virtual bool handle_data(DWORD id, const std::shared_ptr<ff::saved_data_base>& saved_data)
        {
            switch (id)
            {
                case ::make_four_cc('W', 'A', 'V', 'E'):
                    return true;

                case ::make_four_cc('f', 'm', 't', ' '):
                    if (saved_data->loaded_size() >= sizeof(PCMWAVEFORMAT))
                    {
                        std::shared_ptr<ff::reader_base> reader = saved_data->loaded_reader();
                        if (reader)
                        {
                            reader->read(&this->format, sizeof(PCMWAVEFORMAT));
                            return true;
                        }
                    }
                    break;

                case ::make_four_cc('d', 'a', 't', 'a'):
                    this->wav_data = saved_data;
                    return this->wav_data != nullptr;
            }

            // ignore unknown chunks
            return true;
        }
    };
}

std::shared_ptr<ff::saved_data_base> ff::internal::read_wav_file(ff::reader_base& reader, WAVEFORMATEX& format)
{
    ::wav_reader wav;
    if (wav.read(reader) && wav.wav_data != nullptr && wav.format.wFormatTag != 0)
    {
        format = wav.format;
        return wav.wav_data;
    }

    return nullptr;
}
