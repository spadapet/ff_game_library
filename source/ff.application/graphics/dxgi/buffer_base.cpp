#include "pch.h"
#include "graphics/dxgi/buffer_base.h"

std::string_view ff::dxgi::buffer_type_name(ff::dxgi::buffer_type type)
{
    switch (type)
    {
        default: debug_fail_ret_val("");
        case ff::dxgi::buffer_type::none: return "name";
        case ff::dxgi::buffer_type::vertex: return "vertex";
        case ff::dxgi::buffer_type::index: return "index";
        case ff::dxgi::buffer_type::constant: return "constant";
    }
}
