#include "pch.h"
#include "xaml.res.h"

namespace res
{
    void register_xaml()
    {
        ff::data_reader assets_reader(::assets::xaml::data());
        ff::global_resources::add(assets_reader);
    }
}
