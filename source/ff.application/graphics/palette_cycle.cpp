#include "pch.h"
#include "graphics/palette_cycle.h"
#include "graphics/palette_data.h"

ff::palette_cycle::palette_cycle(const std::shared_ptr<ff::palette_data>& data, std::string_view remap_name, float cycles_per_second)
    : data_(data)
    , index_remap_(data ? data->remap(remap_name) : nullptr)
    , index_remap_hash_(this->index_remap_ ? ff::stable_hash_bytes(this->index_remap_->data(), this->index_remap_->size()) : 0)
    , cycles_per_second(static_cast<double>(cycles_per_second))
    , advances(0)
    , current_row_(0)
{}

ff::palette_cycle::operator bool() const
{
    return this->data_ && *this->data_;
}

void ff::palette_cycle::advance()
{
    size_t size = this->data_->row_size();
    this->current_row_ = static_cast<size_t>(++this->advances * this->cycles_per_second * size / ff::constants::advances_per_second<double>()) % size;
}

size_t ff::palette_cycle::current_row() const
{
    return this->current_row_;
}

const ff::dxgi::palette_data_base* ff::palette_cycle::data() const
{
    return this->data_.get();
}

const uint8_t* ff::palette_cycle::index_remap() const
{
    return this->index_remap_ ? this->index_remap_->data() : nullptr;;
}

size_t ff::palette_cycle::index_remap_hash() const
{
    return this->index_remap_hash_;
}
