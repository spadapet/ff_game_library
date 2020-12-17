#pragma once

namespace ff
{
    class resource_value_ref;
    using resource_value_ref_ptr = typename std::shared_ptr<resource_value_ref>;

    class resource_value_ref
    {
    public:

    private:
        std::string name;
        ff::value_ptr value;
        ff::resource_value_ref_ptr new_value_ref;
        // TODO: loading owner...
    };
}
