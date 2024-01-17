#include "pch.h"
#include "animation.h"

ff::animation::animation()
    : play_length_(0)
    , frame_length_(0)
    , frames_per_second_(0)
    , method(ff::animation_keys::method_t::none)
{}

float ff::animation::frame_length() const
{
    return this->play_length_; // this->frame_length_ is the part that loops
}

float ff::animation::frames_per_second() const
{
    return this->frames_per_second_;
}

void ff::animation::frame_events(float start, float end, bool include_start, ff::push_base<ff::animation_event>& events)
{
    if (end <= start || !this->frame_length_)
    {
        return;
    }

    bool loop = ff::flags::has(this->method, ff::animation_keys::method_t::bounds_loop);
    if (loop)
    {
        float length = end - start;
        start = std::fmodf(start, this->frame_length_);
        end = start + length;
    }

    const ff::animation::event_info& find_info = *reinterpret_cast<const ff::animation::event_info*>(&start);
    for (auto i = std::lower_bound(this->events.cbegin(), this->events.cend(), find_info); i != this->events.cend(); i++)
    {
        if (i->frame > end)
        {
            break;
        }
        else if (include_start || i->frame != start)
        {
            events.push(i->public_event);
        }
    }

    if (loop && end > this->frame_length_ && start > 0)
    {
        float loop_end = std::min(end - this->frame_length_, start);
        this->frame_events(0, loop_end, true, events);
    }
}

void ff::animation::draw_frame(ff::dxgi::draw_base& draw, const ff::dxgi::transform& transform, float frame, const ff::dict* params)
{
    if (!ff::animation_keys::adjust_frame(frame, 0.0f, this->frame_length_, this->method))
    {
        return;
    }

    bool push_transform = (transform.rotation != 0);
    const ff::dxgi::transform& draw_transform = push_transform ? ff::dxgi::transform::identity() : transform;

    if (push_transform)
    {
        draw.world_matrix_stack().push();

        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix, transform.matrix());
        draw.world_matrix_stack().transform(matrix);
    }

    for (const ff::animation::visual_info& info : this->visuals)
    {
        float visual_frame = frame - info.start;
        if (!ff::animation_keys::adjust_frame(visual_frame, 0.0f, info.length, info.method))
        {
            continue;
        }

        const ff::animation::cached_visuals_t* visuals = this->get_cached_visuals(info.visual_keys ? info.visual_keys->get_value(visual_frame, params) : nullptr);
        if (!visuals || visuals->empty())
        {
            continue;
        }

        ff::dxgi::transform visual_transform = draw_transform;

        if (info.position_keys)
        {
            ff::value_ptr value = info.position_keys->get_value(visual_frame, params)->try_convert<ff::point_float>();
            if (value)
            {
                visual_transform.position += value->get<ff::point_float>() * draw_transform.scale;
            }
        }

        if (info.scale_keys)
        {
            ff::value_ptr value = info.scale_keys->get_value(visual_frame, params)->try_convert<ff::point_float>();
            if (value)
            {
                visual_transform.scale *= value->get<ff::point_float>();
            }
        }

        if (info.rotate_keys)
        {
            ff::value_ptr value = info.rotate_keys->get_value(visual_frame, params)->try_convert<float>();
            if (value)
            {
                visual_transform.rotation += value->get<float>();
            }
        }

        if (info.color_keys)
        {
            ff::value_ptr value = info.color_keys->get_value(visual_frame, params);
            ff::value_ptr rect_value = value->try_convert<ff::rect_float>();
            ff::value_ptr int_value = value->try_convert<int>();

            if (rect_value)
            {
                DirectX::XMStoreFloat4(&visual_transform.color,
                    DirectX::XMVectorMultiply(
                        DirectX::XMLoadFloat4(&visual_transform.color),
                        DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&rect_value->get<ff::rect_float>()))));
            }
            else if (int_value)
            {
                ff::dxgi::palette_index_to_color(int_value->get<int>(), visual_transform.color);
            }
        }

        for (auto& anim_visual : *visuals)
        {
            float visual_anim_frame = (this->frames_per_second_ != 0.0f) ? visual_frame * anim_visual->frames_per_second() / this->frames_per_second_ : 0.0f;
            anim_visual->draw_frame(draw, visual_transform, visual_anim_frame, params);
        }
    }

    if (push_transform)
    {
        draw.world_matrix_stack().pop();
    }
}

ff::value_ptr ff::animation::frame_value(size_t value_id, float frame, const ff::dict* params)
{
    auto i = this->keys.find(value_id);
    return (i != this->keys.cend()) ? i->second.get_value(frame, params) : nullptr;
}

std::vector<ff::value_ptr> ff::animation::save_events_to_cache() const
{
    std::vector<ff::value_ptr> values;
    values.reserve(this->events.size());

    for (const ff::animation::event_info& event : this->events)
    {
        ff::dict dict;

        dict.set<float>("frame", event.frame);
        dict.set<std::string>("name", event.event_name);
        dict.set<ff::dict>("params", ff::dict(event.params));

        values.push_back(ff::value::create<ff::dict>(std::move(dict)));
    }

    return values;
}

std::vector<ff::value_ptr> ff::animation::save_visuals_to_cache() const
{
    std::vector<ff::value_ptr> values;
    values.reserve(this->visuals.size());

    for (const ff::animation::visual_info& i : this->visuals)
    {
        ff::dict dict;

        dict.set<float>("start", i.start);
        dict.set<float>("length", i.length);
        dict.set<float>("speed", i.speed);
        dict.set_enum("method", i.method);

        if (i.visual_keys)
        {
            dict.set<std::string>("visual", i.visual_keys->name());
        }

        if (i.color_keys)
        {
            dict.set<std::string>("color", i.color_keys->name());
        }

        if (i.position_keys)
        {
            dict.set<std::string>("position", i.position_keys->name());
        }

        if (i.scale_keys)
        {
            dict.set<std::string>("scale", i.scale_keys->name());
        }

        if (i.rotate_keys)
        {
            dict.set<std::string>("rotate", i.rotate_keys->name());
        }

        values.push_back(ff::value::create<ff::dict>(std::move(dict)));
    }

    return values;
}

ff::dict ff::animation::save_keys_to_cache() const
{
    ff::dict dict;

    for (auto& i : this->keys)
    {
        const ff::animation_keys& key = i.second;
        dict.set<ff::dict>(key.name(), key.save_to_cache());
    }

    return dict;
}

bool ff::animation::load(const ff::dict& dict, bool from_source, ff::resource_load_context& context)
{
    this->frame_length_ = dict.get<float>("length");
    this->play_length_ = dict.get<float>("play_length", this->frame_length_);
    this->frames_per_second_ = dict.get<float>("fps");
    this->method = ff::animation_keys::load_method(dict, from_source);

    return this->load_keys(dict.get<ff::dict>("keys"), from_source, context) &&
        this->load_visuals(dict.get<std::vector<ff::value_ptr>>("visuals"), from_source, context) &&
        this->load_events(dict.get<std::vector<ff::value_ptr>>("events"), from_source, context);
}

bool ff::animation::load_events(const std::vector<ff::value_ptr>& values, bool from_source, ff::resource_load_context& context)
{
    for (ff::value_ptr value : values)
    {
        ff::value_ptr dict_value = value->try_convert<ff::dict>();
        if (dict_value)
        {
            const ff::dict& dict = dict_value->get<ff::dict>();

            ff::animation::event_info event{};
            event.frame = dict.get<float>("frame");
            event.event_name = dict.get<std::string>("name");
            event.params = dict.get<ff::dict>("params");

            assert(!event.event_name.empty());

            this->events.push_back(std::move(event));
        }
    }

    for (ff::animation::event_info& event : this->events)
    {
        event.public_event.animation = this;
        event.public_event.event_id = ff::stable_hash_func(event.event_name);
        event.public_event.params = &event.params;
    }

    return true;
}

bool ff::animation::load_visuals(const std::vector<ff::value_ptr>& values, bool from_source, ff::resource_load_context& context)
{
    for (ff::value_ptr value : values)
    {
        ff::value_ptr dict_value = value->try_convert<ff::dict>();
        if (dict_value)
        {
            const ff::dict& dict = dict_value->get<ff::dict>();

            ff::animation::visual_info info{};
            info.start = dict.get<float>("start");
            info.length = dict.get<float>("length", std::max(this->frame_length_ - info.start, 0.0f));
            info.speed = dict.get<float>("speed", 1.0f);
            info.method = ff::animation_keys::load_method(dict, from_source);

            std::string visual_keys = dict.get<std::string>("visual");
            std::string color_keys = dict.get<std::string>("color");
            std::string position_keys = dict.get<std::string>("position");
            std::string scale_keys = dict.get<std::string>("scale");
            std::string rotate_keys = dict.get<std::string>("rotate");

            auto visual_iter = visual_keys.size() ? this->keys.find(ff::stable_hash_func(visual_keys)) : this->keys.cend();
            auto color_iter = color_keys.size() ? this->keys.find(ff::stable_hash_func(color_keys)) : this->keys.cend();
            auto position_iter = position_keys.size() ? this->keys.find(ff::stable_hash_func(position_keys)) : this->keys.cend();
            auto scale_iter = scale_keys.size() ? this->keys.find(ff::stable_hash_func(scale_keys)) : this->keys.cend();
            auto rotate_iter = rotate_keys.size() ? this->keys.find(ff::stable_hash_func(rotate_keys)) : this->keys.cend();

            if ((visual_keys.empty() || visual_iter != this->keys.cend()) &&
                (color_keys.empty() || color_iter != this->keys.cend()) &&
                (position_keys.empty() || position_iter != this->keys.cend()) &&
                (scale_keys.empty() || scale_iter != this->keys.cend()) &&
                (rotate_keys.empty() || rotate_iter != this->keys.cend()))
            {
                info.visual_keys = (visual_iter != this->keys.cend()) ? &visual_iter->second : nullptr;
                info.color_keys = (color_iter != this->keys.cend()) ? &color_iter->second : nullptr;
                info.position_keys = (position_iter != this->keys.cend()) ? &position_iter->second : nullptr;
                info.scale_keys = (scale_iter != this->keys.cend()) ? &scale_iter->second : nullptr;
                info.rotate_keys = (rotate_iter != this->keys.cend()) ? &rotate_iter->second : nullptr;

                this->visuals.push_back(std::move(info));
            }
        }
    }

    return true;
}

bool ff::animation::load_keys(const ff::dict& values, bool from_source, ff::resource_load_context& context)
{
    for (auto& i : values)
    {
        ff::dict dict = i.second->get<ff::dict>();
        ff::animation_keys keys = from_source
            ? ff::animation_keys::load_from_source(i.first, dict, context)
            : ff::animation_keys::load_from_cache(dict);

        this->keys.try_emplace(ff::stable_hash_func(i.first), std::move(keys));
    }

    return true;
}

const ff::animation::cached_visuals_t* ff::animation::get_cached_visuals(const ff::value_ptr& value)
{
    if (!value)
    {
        return nullptr;
    }

    auto i = this->cached_visuals.find(value);
    if (i == this->cached_visuals.cend())
    {
        ff::animation::cached_visuals_t visuals;

        auto anim = std::dynamic_pointer_cast<ff::animation_base>(value->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>());
        if (anim)
        {
            visuals.push_back(anim);
        }
        else if (value->is_type<std::vector<ff::value_ptr>>())
        {
            auto values = value->get<std::vector<ff::value_ptr>>();
            visuals.reserve(values.size());

            for (ff::value_ptr child_value : values)
            {
                anim = std::dynamic_pointer_cast<ff::animation_base>(child_value->convert_or_default<ff::resource_object_base>()->get<ff::resource_object_base>());
                if (anim)
                {
                    visuals.push_back(anim);
                }
            }
        }

        assert(!visuals.empty());
        i = this->cached_visuals.try_emplace(ff::value_ptr(value), std::move(visuals)).first;
    }

    return &i->second;
}

bool ff::animation::save_to_cache(ff::dict& dict, bool& allow_compress) const
{
    dict.set<float>("length", this->frame_length_);
    dict.set<float>("play_length", this->play_length_);
    dict.set<float>("fps", this->frames_per_second_);
    dict.set_enum("method", this->method);

    dict.set<std::vector<ff::value_ptr>>("events", this->save_events_to_cache());
    dict.set<std::vector<ff::value_ptr>>("visuals", this->save_visuals_to_cache());
    dict.set<ff::dict>("keys", this->save_keys_to_cache());

    return true;
}

std::shared_ptr<ff::resource_object_base> ff::internal::animation_factory::load_from_source(const ff::dict& dict, ff::resource_load_context& context) const
{
    auto result = std::make_shared<ff::animation>();
    return result->load(dict, true, context) ? result : nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::animation_factory::load_from_cache(const ff::dict& dict) const
{
    auto result = std::make_shared<ff::animation>();
    return result->load(dict, false, ff::resource_load_context::null()) ? result : nullptr;
}

bool ff::animation::visual_info::operator<(const ff::animation::visual_info& other) const
{
    return this->start < other.start;
}

bool ff::animation::event_info::operator<(const ff::animation::event_info& other) const
{
    return this->frame < other.frame;
}

ff::create_animation::create_animation(float length, float frames_per_second, ff::animation_keys::method_t method)
{
    this->dict.set<float>("length", length);
    this->dict.set<float>("fps", frames_per_second);
    this->dict.set_enum("method", method);
}

void ff::create_animation::add_keys(const ff::create_animation_keys& key)
{
    std::string name;
    ff::dict dict = key.create_source_dict(name);
    this->keys.set<ff::dict>(name, std::move(dict));
}

void ff::create_animation::add_event(float frame, std::string_view name, const ff::dict* params)
{
    ff::dict dict;
    dict.set<float>("frame", frame);
    dict.set<std::string>("name", std::string(name));

    if (params)
    {
        dict.set<ff::dict>("params", ff::dict(*params));
    }

    this->events.push_back(ff::value::create<ff::dict>(std::move(dict)));
}

void ff::create_animation::add_visual(float start, float length, float speed, ff::animation_keys::method_t method, std::string_view visual_keys, std::string_view color_keys, std::string_view position_keys, std::string_view scale_keys, std::string_view rotate_keys)
{
    ff::dict dict;
    dict.set<float>("start", start);
    dict.set<float>("length", length);
    dict.set<float>("speed", speed);
    dict.set_enum("method", method);
    dict.set<std::string>("visual", std::string(visual_keys));
    dict.set<std::string>("color", std::string(color_keys));
    dict.set<std::string>("position", std::string(position_keys));
    dict.set<std::string>("scale", std::string(scale_keys));
    dict.set<std::string>("rotate", std::string(rotate_keys));

    this->visuals.push_back(ff::value::create<ff::dict>(std::move(dict)));
}

std::shared_ptr<ff::animation> ff::create_animation::create() const
{
    ff::dict dict = this->dict;
    dict.set<ff::dict>("keys", ff::dict(this->keys));
    dict.set<std::vector<ff::value_ptr>>("visuals", std::vector<ff::value_ptr>(this->visuals));
    dict.set<std::vector<ff::value_ptr>>("events", std::vector<ff::value_ptr>(this->events));

    const ff::resource_object_factory_base* factory = ff::resource_object_base::get_factory(typeid(ff::animation));
    assert(factory);

    return factory ? std::dynamic_pointer_cast<ff::animation>(factory->load_from_source(dict, ff::resource_load_context::null())) : nullptr;
}
