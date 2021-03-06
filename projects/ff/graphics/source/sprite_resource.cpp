#include "pch.h"
#include "draw_base.h"
#include "sprite_list.h"
#include "sprite_resource.h"

static ff::sprite_data empty_sprite_data{};

ff::sprite_resource::sprite_resource(std::string&& name, const std::shared_ptr<ff::resource>& source)
    : name_(std::move(name))
    , source(source)
    , sprite_data_(&::empty_sprite_data)
{}

std::string_view ff::sprite_resource::name() const
{
    return this->name_;
}

const ff::sprite_data& ff::sprite_resource::sprite_data() const
{
    return *this->sprite_data_;
}

float ff::sprite_resource::frame_length() const
{
    return 0.0f;
}

float ff::sprite_resource::frames_per_second() const
{
    return 0.0f;
}

void ff::sprite_resource::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{}

void ff::sprite_resource::draw_frame(ff::draw_base& draw, const ff::transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data(), transform);
}

void ff::sprite_resource::draw_frame(ff::draw_base& draw, const ff::pixel_transform& transform, float frame, const ff::dict* params)
{
    draw.draw_sprite(this->sprite_data(), transform);
}

ff::value_ptr ff::sprite_resource::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    return nullptr;
}

void ff::sprite_resource::advance_animation(ff::push_base<ff::animation_event>* events)
{}

void ff::sprite_resource::draw_animation(ff::draw_base& draw, const ff::transform& transform) const
{
    draw.draw_sprite(this->sprite_data(), transform);
}

void ff::sprite_resource::draw_animation(ff::draw_base& draw, const ff::pixel_transform& transform) const
{
    draw.draw_sprite(this->sprite_data(), transform);
}

float ff::sprite_resource::animation_frame() const
{
    return 0.0f;
}

const ff::animation_base* ff::sprite_resource::animation() const
{
    return this;
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

bool ff::sprite_resource::save_to_cache(ff::dict& dict, bool& allow_compress) const
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
