#pragma once

namespace ff::internal
{
    size_t hash_bytes(const void* data, size_t size) noexcept;
}

namespace ff
{
    /// <summary>
    /// Replacement for std::hash that is always stable, the hashes could be persisted.
    /// </summary>
    template<class T>
    struct hash
    {
        size_t operator()(const T& value) const noexcept
        {
            return ff::internal::hash_bytes(&value, sizeof(T));
        }
    };

    template<>
    struct hash<std::type_index>
    {
        size_t operator()(const std::type_index& value) const noexcept
        {
            return value.hash_code();
        }
    };

    template<class Elem, class Traits>
    struct hash<std::basic_string_view<Elem, Traits>>
    {
        size_t operator()(const std::basic_string_view<Elem, Traits>& value) const noexcept
        {
            return ff::internal::hash_bytes(value.data(), value.size() * sizeof(Elem));
        }
    };

    template<class Elem, class Traits, class Alloc>
    struct hash<std::basic_string<Elem, Traits, Alloc>>
    {
        size_t operator()(const std::basic_string<Elem, Traits, Alloc>& value) const noexcept
        {
            return ff::internal::hash_bytes(value.data(), value.size() * sizeof(Elem));
        }
    };

    template<>
    struct hash<std::filesystem::path>
    {
        size_t operator()(const std::filesystem::path& value) const noexcept
        {
            return ff::hash<std::wstring>()(value.native());
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
    size_t hash_func(const T& value)
    {
        return ff::hash<T>()(value);
    }
}

namespace ff::internal
{
    template<class Hash, class Key, class T>
    struct pair_key_hash
    {
        size_t operator()(const std::pair<const Key, T>& pair) const noexcept
        {
            return Hash()(pair.first);
        }
    };

    template<class Compare, class Key, class T>
    struct pair_key_compare
    {
        size_t operator()(const std::pair<const Key, T>& first, const std::pair<const Key, T>& second) const noexcept
        {
            return Compare()(first.first, second.first);
        }
    };
}
