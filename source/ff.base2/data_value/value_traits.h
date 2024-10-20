#pragma once

namespace ff
{
    class value;
}

namespace ff::type
{
    template<class T, class Enabled = void>
    struct value_traits;

    template<class T>
    struct value_derived_traits
    {
        using value_derived_type = typename T;
        using get_type = typename std::invoke_result_t<decltype(&T::get), T>;
        using raw_type = typename std::remove_cv_t<std::remove_reference_t<get_type>>;
    };

    template<class T>
    struct value_traits<T, std::enable_if_t<std::is_base_of_v<ff::value, T>>> : public value_derived_traits<T>
    {};
}
