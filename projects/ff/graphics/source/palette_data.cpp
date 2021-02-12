#include "pch.h"
#include "dx11_texture.h"
#include "palette_data.h"

ff::palette_data::palette_data(DirectX::ScratchImage&& scratch, std::unordered_map<std::string, std::shared_ptr<ff::data_base>>&& name_to_remap)
    : name_to_remap(name_to_remap)
{
    if (scratch.GetImageCount())
    {
        const DirectX::Image& image = *scratch.GetImages();
        if (image.width == ff::constants::palette_size)
        {
            this->row_hashes.reserve(image.height);

            for (const uint8_t* start = image.pixels, *cur = start, *end = start + image.height * image.rowPitch; cur < end; cur += image.rowPitch)
            {
                this->row_hashes.push_back(ff::stable_hash_bytes(cur, ff::constants::palette_row_bytes));
            }

            this->texture_ = std::make_shared<ff::dx11_texture>(std::make_shared<DirectX::ScratchImage>(std::move(scratch)));
        }
    }
}

ff::palette_data::operator bool() const
{
    return this->texture_ && *this->texture_;
}

size_t ff::palette_data::row_size() const
{
    return this->row_hashes.size();
}

size_t ff::palette_data::row_hash(size_t index) const
{
    return this->row_hashes[index];
}

const std::shared_ptr<ff::dx11_texture> ff::palette_data::texture()
{
    return this->texture_;
}

const uint8_t* ff::palette_data::remap(std::string_view name) const
{
    auto i = this->name_to_remap.find(std::string(name));
    return i != this->name_to_remap.cend() ? i->second->data() : nullptr;
}

size_t ff::palette_data::current_row() const
{
    return 0;
}

ff::palette_data* ff::palette_data::data()
{
    return this;
}

const uint8_t* ff::palette_data::index_remap() const
{
    return nullptr;
}

size_t ff::palette_data::index_remap_hash() const
{
    return 0;
}

bool ff::palette_data::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    return false;
}

std::shared_ptr<ff::resource_object_base> ff::internal::palette_data_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return std::shared_ptr<resource_object_base>();
}

std::shared_ptr<ff::resource_object_base> ff::internal::palette_data_factory::load_from_cache(const ff::dict& dict) const
{
    return std::shared_ptr<resource_object_base>();
}
