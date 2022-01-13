#include "pch.h"
#include "xaml.res.h"

namespace res
{
    void register_xaml()
    {
        ff::global_resources::add(::assets::xaml::data());
    }
}
