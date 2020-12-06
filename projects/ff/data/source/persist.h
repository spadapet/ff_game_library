#pragma once

namespace ff
{
    class data_base;
    class reader_base;
    class writer_base;
}

namespace ff
{
    bool load_bytes(reader_base& reader, void* data, size_t size);
    bool load_bytes(reader_base& reader, size_t size, std::shared_ptr<data_base>& data);

    bool save_bytes(writer_base& writer, const void* data, size_t size);
    bool save_bytes(writer_base& writer, const data_base& data);

    template<class T>
    bool load(reader_base& reader, T& data)
    {
        return ff::persist<T>::load(reader, data);
    }

    template<class T>
    bool save(writer_base& writer, const T& data)
    {
        return ff::persist<T>::save(writer, data);
    }

    template<class T, class Enabled = void>
    struct persist;

    template<class T>
    struct persist<T, std::enable_if_t<std::is_trivially_copyable_v<T>>>
    {
        static bool load(reader_base& reader, T& data)
        {
            return ff::load_bytes(reader, &data, sizeof(T));
        }

        static bool save(writer_base& writer, const T& data)
        {
            return ff::save_bytes(writer, &data, sizeof(T));
        }
    };

    template<>
    struct persist<size_t>
    {
        static bool load(reader_base& reader, size_t& data)
        {
            uint64_t data64;
            if (ff::load_bytes(reader, &data64, sizeof(data64)))
            {
                data = static_cast<size_t>(data64);
                return true;
            }

            return false;
        }

        static bool save(writer_base& writer, const size_t& data)
        {
            uint64_t data64 = static_cast<uint64_t>(data);
            return ff::save_bytes(writer, &data64, sizeof(data64));
        }
    };

    template<class Elem, class Traits, class Alloc>
    struct persist<std::basic_string<Elem, Traits, Alloc>>
    {
        static bool load(reader_base& reader, std::basic_string<Elem, Traits, Alloc>& data)
        {
            size_t length;
            if (ff::load(reader, length))
            {
                data.resize(length);
                return ff::load_bytes(reader, data.data(), length * sizeof(Elem));
            }

            return false;
        }

        static bool save(writer_base& writer, const std::basic_string<Elem, Traits, Alloc>& data)
        {
            size_t length = data.length();
            return ff::save(writer, length) && ff::save_bytes(writer, data.data(), length * sizeof(Elem));
        }
    };
}
