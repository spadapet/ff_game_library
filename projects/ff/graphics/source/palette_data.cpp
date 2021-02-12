#include "pch.h"
#include "dx11_texture.h"
#include "dxgi_util.h"
#include "palette_data.h"
#include "texture_util.h"

ff::palette_data::palette_data(DirectX::ScratchImage&& scratch, std::unordered_map<std::string, std::shared_ptr<ff::data_base>>&& name_to_remap)
    : name_to_remap(std::move(name_to_remap))
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

ff::palette_data::palette_data(std::shared_ptr<ff::dx11_texture>&& texture, std::vector<size_t>&& row_hashes, std::unordered_map<std::string, std::shared_ptr<ff::data_base>>&& name_to_remap)
    : texture_(std::move(texture))
    , row_hashes(std::move(row_hashes))
    , name_to_remap(std::move(name_to_remap))
{
    assert(this->texture_ && this->texture_->size().cast<size_t>().x == ff::constants::palette_size);
}

ff::palette_data::operator bool() const
{
    return this->texture_ && *this->texture_ && this->texture_->size().cast<size_t>().x == ff::constants::palette_size;
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

std::shared_ptr<ff::data_base> ff::palette_data::remap(std::string_view name) const
{
    auto i = this->name_to_remap.find(std::string(name));
    return i != this->name_to_remap.cend() ? i->second : nullptr;
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
    ff::dict remaps;
    remaps.reserve(this->name_to_remap.size());

    for (auto& i : this->name_to_remap)
    {
        remaps.set<ff::data_base>(i.first, i.second, ff::saved_data_type::none);
    }

    dict.set<ff::resource_object_base>("texture", std::dynamic_pointer_cast<ff::resource_object_base>(this->texture_));
    dict.set_bytes("hashes", this->row_hashes.data(), ff::vector_byte_size(this->row_hashes));
    dict.set<size_t>("size", this->row_hashes.size());
    dict.set<ff::dict>("remaps", std::move(remaps));

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::palette_data_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    std::filesystem::path full_path = dict.get<std::string>("file");
    std::shared_ptr<DirectX::ScratchImage> scratch_palette;
    std::shared_ptr<DirectX::ScratchImage> scratch_data = ff::internal::load_texture_data(full_path, ff::internal::DEFAULT_FORMAT, 1, scratch_palette);
    std::unordered_map<std::string, std::shared_ptr<ff::data_base>> name_to_remap;

    if (!scratch_data || !scratch_data->GetImageCount())
    {
        assert(false);
        return nullptr;
    }

    for (auto& remap_pair : dict.get<ff::dict>("remaps"))
    {
        std::vector<uint8_t> remap;
        remap.resize(ff::constants::palette_size);

        for (size_t i = 0; i < ff::constants::palette_size; i++)
        {
            remap[i] = static_cast<uint8_t>(i);
        }

        for (auto& value : remap_pair.second->get<std::vector<ff::value_ptr>>())
        {
            ff::value_ptr point_value = value->try_convert<ff::point_int>();
            assert(point_value);

            if (point_value)
            {
                ff::point_int point = point_value->get<ff::point_int>();
                size_t i = static_cast<size_t>(point.x & 0xFF);
                remap[i] = static_cast<uint8_t>(point.y & 0xFF);
            }
        }

        std::shared_ptr<ff::data_base> remap_data = std::make_shared<ff::data_vector>(std::move(remap));
        name_to_remap.try_emplace(std::string(remap_pair.first), std::move(remap_data));
    }

    auto result = std::make_shared<ff::palette_data>(std::move(*scratch_data), std::move(name_to_remap));
    return *result ? result : nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::palette_data_factory::load_from_cache(const ff::dict& dict) const
{
    std::shared_ptr<ff::dx11_texture> texture = std::dynamic_pointer_cast<ff::dx11_texture>(dict.get<ff::resource_object_base>("texture"));
    std::unordered_map<std::string, std::shared_ptr<ff::data_base>> name_to_remap;
    std::vector<size_t> row_hashes;

    row_hashes.resize(dict.get<size_t>("size"));
    if (!dict.get_bytes("hashes", row_hashes.data(), ff::vector_byte_size(row_hashes)))
    {
        assert(false);
        return nullptr;
    }

    for (auto& i : dict.get<ff::dict>("remaps"))
    {
        auto data = i.second->get<ff::data_base>();
        if (data && data->size() == ff::constants::palette_size)
        {
            name_to_remap.try_emplace(std::string(i.first), std::move(data));
        }
        else
        {
            assert(false);
        }
    }

    auto result = std::make_shared<ff::palette_data>(std::move(texture), std::move(row_hashes), std::move(name_to_remap));
    return *result ? result : nullptr;
}
