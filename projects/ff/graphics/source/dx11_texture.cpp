#include "pch.h"
#include "dx11_texture.h"

ff::dx11_texture_o::dx11_texture_o()
{}

ff::dict ff::dx11_texture_o::resource_get_siblings(const std::shared_ptr<resource>&self) const
{
    return ff::dict();
}

bool ff::dx11_texture_o::resource_save_to_file(const std::filesystem::path& directory_path, std::string_view name) const
{
    return false;
}

bool ff::dx11_texture_o::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    return false;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_factory::load_from_cache(const ff::dict& dict) const
{
    return nullptr;
}
