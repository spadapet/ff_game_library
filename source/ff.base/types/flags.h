#pragma once

/// <summary>
/// Useful enum flags helper functions
/// </summary>
namespace ff::flags
{
    template<class T>
    constexpr T get(T state, T flags)
    {
        using TI = std::underlying_type_t<T>;
        const TI istate = static_cast<TI>(state);
        const TI iflags = static_cast<TI>(flags);
        return static_cast<T>(istate & iflags);
    }

    template<class T>
    constexpr bool has(T state, T check)
    {
        using TI = std::underlying_type_t<T>;
        return (static_cast<TI>(state) & static_cast<TI>(check)) == static_cast<TI>(check);
    }

    template<class T>
    constexpr bool has_any(T state, T check)
    {
        using TI = std::underlying_type_t<T>;
        return (static_cast<TI>(state) & static_cast<TI>(check)) != static_cast<TI>(0);
    }

    template<class T>
    constexpr T set(T state, T set, bool value)
    {
        using TI = std::underlying_type_t<T>;
        const TI istate = static_cast<TI>(state);
        const TI iset = static_cast<TI>(set);
        const TI ival = static_cast<TI>(value);

        return static_cast<T>((istate & ~iset) | (iset * ival));
    }

    template<class T>
    constexpr T set(T state, T set)
    {
        using TI = std::underlying_type_t<T>;
        return static_cast<T>(static_cast<TI>(state) | static_cast<TI>(set));
    }

    template<class T>
    constexpr T clear(T state, T clear)
    {
        using TI = std::underlying_type_t<T>;
        return static_cast<T>(static_cast<TI>(state) & ~static_cast<TI>(clear));
    }

    template<class T>
    constexpr T toggle(T state, T toggle)
    {
        using TI = std::underlying_type_t<T>;
        return static_cast<T>(static_cast<TI>(state) ^ static_cast<TI>(toggle));
    }

    template<class T, class... Ts>
    constexpr T combine(T first, Ts... rest)
    {
        static_assert(std::is_enum_v<T>, "T must be an enum type");
        static_assert((std::is_same_v<T, Ts> && ...), "All types in Ts must be the same as T");

        using TI = std::underlying_type_t<T>;
        return static_cast<T>((static_cast<TI>(first) | ... | static_cast<TI>(rest)));
    }
}
