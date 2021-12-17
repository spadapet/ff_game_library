#pragma once

namespace ff
{
    struct stable_hash_data_t
    {
        stable_hash_data_t();
        stable_hash_data_t(size_t data_size);

        size_t hash() const;
        operator size_t() const;

    private:
        friend ff::stable_hash_data_t stable_hash_incremental(const void* data, size_t size, const ff::stable_hash_data_t& prev_data) noexcept;

        stable_hash_data_t(uint32_t a, uint32_t b, uint32_t c);

        uint32_t a, b, c;
    };

    ff::stable_hash_data_t stable_hash_incremental(const void* data, size_t size, const ff::stable_hash_data_t& prev_data = ff::stable_hash_data_t()) noexcept;
    size_t stable_hash_bytes(const void* data, size_t size) noexcept;

    /// <summary>
    /// Replacement for std::hash that is always stable, the hashes could be persisted.
    /// </summary>
    template<class T>
    struct stable_hash
    {
        size_t operator()(const T& value) const noexcept
        {
            return ff::stable_hash_bytes(&value, sizeof(T));
        }
    };

    template<>
    struct stable_hash<std::type_index>
    {
        size_t operator()(const std::type_index& value) const noexcept
        {
            return value.hash_code();
        }
    };

    template<class Elem, class Traits>
    struct stable_hash<std::basic_string_view<Elem, Traits>>
    {
        size_t operator()(const std::basic_string_view<Elem, Traits>& value) const noexcept
        {
            return ff::stable_hash_bytes(value.data(), value.size() * sizeof(Elem));
        }
    };

    template<class Elem, class Traits, class Alloc>
    struct stable_hash<std::basic_string<Elem, Traits, Alloc>>
    {
        size_t operator()(const std::basic_string<Elem, Traits, Alloc>& value) const noexcept
        {
            return ff::stable_hash_bytes(value.data(), value.size() * sizeof(Elem));
        }
    };

    template<>
    struct stable_hash<std::filesystem::path>
    {
        size_t operator()(const std::filesystem::path& value) const noexcept
        {
            return ff::stable_hash<std::wstring>()(value.native());
        }
    };

    /// <summary>
    /// Treats a value as its own hash by attempting to cast to size_t
    /// </summary>
    template<class T>
    struct no_hash
    {
        size_t operator()(const T& value) const noexcept
        {
            return static_cast<size_t>(value);
        }
    };

    template<class T>
    size_t stable_hash_func(const T& value)
    {
        return ff::stable_hash<T>()(value);
    }
}
