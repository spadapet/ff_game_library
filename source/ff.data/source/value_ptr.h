#pragma once

namespace ff
{
    class value;
    using value_ptr = typename ff::intrusive_ptr<const value>;
    using value_vector = typename std::vector<value_ptr>;
}
