#pragma once

namespace ff::data
{
    class data_base;
    class reader_base;
    class writer_base;
}

namespace ff::data
{
    bool load_bytes(reader_base& reader, void* data, size_t size);
    bool load_bytes(reader_base& reader, size_t size, std::shared_ptr<data_base>& data);

    bool save_bytes(writer_base& writer, const void* data, size_t size);
    bool save_bytes(writer_base& writer, const data_base& data);

    template<class T>
    bool load(reader_base& reader, T& data)
    {
        return ff::data::persist<T>::load(reader, data);
    }

    template<class T>
    bool save(writer_base& writer, const T& data)
    {
        return ff::data::persist<T>::save(writer, data);
    }

    template<class T, class Enabled = void>
    struct persist;

    template<class T>
    struct persist<T, std::enable_if_t<std::is_trivially_copyable_v<T>>>
    {
        static bool load(reader_base& reader, T& data)
        {
            return ff::data::load_bytes(reader, &data, sizeof(T));
        }

        static bool save(writer_base& writer, const T& data)
        {
            return ff::data::save_bytes(writer, &data, sizeof(T));
        }
    };

    template<>
    struct persist<size_t>
    {
        static bool load(reader_base& reader, size_t& data)
        {
            uint64_t data64;
            if (ff::data::load(reader, data64))
            {
                data = static_cast<size_t>(data64);
                return true;
            }

            return false;
        }

        static bool save(writer_base& writer, const size_t& data)
        {
            uint64_t data64 = static_cast<uint64_t>(data);
            return ff::data::save(writer, data64);
        }
    };

    template<class Elem, class Traits, class Alloc>
    struct persist<std::basic_string<Elem, Traits, Alloc>>
    {
        static bool load(reader_base& reader, std::basic_string<Elem, Traits, Alloc>& data)
        {
            size_t length;
            if (ff::data::load(reader, length))
            {
                size_t byte_size = length * sizeof(Elem);
                data.resize(length);
                return ff::data::load(reader, data.data(), byte_size);
            }
        }

        static bool save(writer_base& writer, const std::basic_string<Elem, Traits, Alloc>& data)
        {
            size_t length = data.length();
            size_t byte_size = length * sizeof(Elem);
            return ff::data::save(writer, length) && ff::data::save(writer, data.data(), byte_size);
        }
    };
}
