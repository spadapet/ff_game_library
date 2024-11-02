#include "pch.h"
#include "dxgi/draw_base.h"
#include "graphics/sprite_list.h"
#include "graphics/sprite_resource.h"

static ff::dxgi::sprite_data empty_sprite_data{};

ff::sprite_resource::sprite_resource(std::string&& name, const std::shared_ptr<ff::resource>& source)
    : name_(std::move(name))
    , source(source)
    , sprite_data_(&::empty_sprite_data)
{}

std::string_view ff::sprite_resource::name() const
{
    return this->name_;
}

const ff::dxgi::sprite_data& ff::sprite_resource::sprite_data() const
{
    return *this->sprite_data_;
}

void ff::sprite_resource::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    this->draw_animation(draw, transform);
}

void ff::sprite_resource::draw_animation(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform) const
{
    draw.draw_sprite(this->sprite_data(), transform);
}

bool ff::sprite_resource::resource_load_complete(bool from_source)
{
    std::shared_ptr<ff::sprite_base> source_sprite = std::dynamic_pointer_cast<ff::sprite_base>(this->source.object());
    if (source_sprite)
    {
        this->sprite_data_ = &source_sprite->sprite_data();
        return true;
    }

    std::shared_ptr<ff::sprite_list> source_sprite_list = std::dynamic_pointer_cast<ff::sprite_list>(this->source.object());
    if (source_sprite_list && source_sprite_list->get(this->name_))
    {
        this->sprite_data_ = &source_sprite_list->get(this->name_)->sprite_data();
        return true;
    }

    assert(false);
    return false;
}

std::vector<std::shared_ptr<ff::resource>> ff::sprite_resource::resource_get_dependencies() const
{
    return std::vector<std::shared_ptr<ff::resource>>
    {
        this->source.resource()
    };
}

bool ff::sprite_resource::save_to_cache(ff::dict& dict) const
{
    dict.set<ff::resource>("source", this->source.resource());
    dict.set<std::string>("name", this->name_);
    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_resource_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    return ff::internal::sprite_resource_factory::load_from_cache(dict);
}

std::shared_ptr<ff::resource_object_base> ff::internal::sprite_resource_factory::load_from_cache(const ff::dict& dict) const
{
    std::shared_ptr<ff::resource> source = dict.get<ff::resource>("source");
    std::string name = dict.get<std::string>("name");

    return std::make_shared<sprite_resource>(std::move(name), source);
}
