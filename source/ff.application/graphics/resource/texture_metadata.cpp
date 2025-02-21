#include "pch.h"
#include "graphics/dxgi/format_util.h"
#include "graphics/resource/texture_metadata.h"

ff::texture_metadata::texture_metadata(ff::point_size size, size_t mip_count, size_t array_size, size_t sample_count, DXGI_FORMAT format)
    : size_(size)
    , mip_count_(mip_count)
    , array_size_(array_size)
    , sample_count_(sample_count)
    , format_(format)
{}

ff::point_size ff::texture_metadata::size() const
{
    return this->size_;
}

size_t ff::texture_metadata::mip_count() const
{
    return this->mip_count_;
}

size_t ff::texture_metadata::array_size() const
{
    return this->array_size_;
}

size_t ff::texture_metadata::sample_count() const
{
    return this->sample_count_;
}

DXGI_FORMAT ff::texture_metadata::format() const
{
    return this->format_;
}

bool ff::texture_metadata::save_to_cache(ff::dict& dict) const
{
    dict.set<ff::point_size>("size", this->size_);
    dict.set<size_t>("mip_count", this->mip_count_);
    dict.set<size_t>("array_size", this->array_size_);
    dict.set<size_t>("sample_count", this->sample_count_);
    dict.set_enum<DXGI_FORMAT>("format", this->format_);

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_metadata_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return std::make_shared<ff::texture_metadata>(
        dict.get<ff::point_size>("size"),
        dict.get<size_t>("mip_count"),
        dict.get<size_t>("array_size"),
        dict.get<size_t>("sample_count"),
        ff::dxgi::parse_format(dict.get<std::string>("format")));
}

std::shared_ptr<ff::resource_object_base> ff::internal::texture_metadata_factory::load_from_cache(const ff::dict& dict) const
{
    return std::make_shared<ff::texture_metadata>(
        dict.get<ff::point_size>("size"),
        dict.get<size_t>("mip_count"),
        dict.get<size_t>("array_size"),
        dict.get<size_t>("sample_count"),
        dict.get_enum<DXGI_FORMAT>("format"));
}
