#pragma once

/// <summary>
/// Useful enum flags helper functions
/// </summary>
namespace ff::flags
{
    template<class T>
    T get(T state, T flags)
    {
        typedef std::underlying_type_t<T> TI;
        const TI istate = static_cast<TI>(state);
        const TI iflags = static_cast<TI>(flags);
        return static_cast<T>(istate & iflags);
    }

    template<class T>
    bool has(T state, T check)
    {
        typedef std::underlying_type_t<T> TI;
        return (static_cast<TI>(state) & static_cast<TI>(check)) == static_cast<TI>(check);
    }

    template<class T>
    bool has_any(T state, T check)
    {
        typedef std::underlying_type_t<T> TI;
        return (static_cast<TI>(state) & static_cast<TI>(check)) != static_cast<TI>(0);
    }

    template<class T>
    T set(T state, T set, bool value)
    {
        typedef std::underlying_type_t<T> TI;
        const TI istate = static_cast<TI>(state);
        const TI iset = static_cast<TI>(set);
        const TI ival = static_cast<TI>(value);

        return static_cast<T>((istate & ~iset) | (iset * ival));
    }

    template<class T>
    T set(T state, T set)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(state) | static_cast<TI>(set));
    }

    template<class T>
    T clear(T state, T clear)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(state) & ~static_cast<TI>(clear));
    }

    template<class T>
    T toggle(T state, T toggle)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(state) ^ static_cast<TI>(toggle));
    }

    template<class T>
    T combine(T f1, T f2)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(f1) | static_cast<TI>(f2));
    }

    template<class T>
    T combine(T f1, T f2, T f3)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(f1) | static_cast<TI>(f2) | static_cast<TI>(f3));
    }

    template<class T>
    T combine(T f1, T f2, T f3, T f4)
    {
        typedef std::underlying_type_t<T> TI;
        return static_cast<T>(static_cast<TI>(f1) | static_cast<TI>(f2) | static_cast<TI>(f3) | static_cast<TI>(f4));
    }
}
