#include "pch.h"
#include "key_frames.h"

bool ff::key_frames::key_frame::operator<(const key_frame& other) const
{
    return this->frame < other.frame;
}

bool ff::key_frames::key_frame::operator<(float frame) const
{
    return this->frame < frame;
}

ff::key_frames::key_frames()
    : start_(0)
    , length_(0)
    , method(method_t::none)
{}

ff::value_ptr ff::key_frames::get_value(float frame, const ff::dict* params)
{
    if (this->keys.size() && this->adjust_frame(frame, this->start_, this->length_, this->method))
    {
        auto key_iter = std::lower_bound(this->keys.cbegin(), this->keys.cend(), frame);
        if (key_iter == this->keys.cend())
        {
            return this->keys.back().value;
        }
        else if (key_iter == this->keys.cbegin() || key_iter->frame == frame)
        {
            return key_iter->value;
        }
        else
        {
            const key_frame& prev_key = *std::prev(key_iter);
            const key_frame& next_key = *key_iter;

            float time = (frame - prev_key.frame) / (next_key.frame - prev_key.frame);
            return this->interpolate(prev_key, next_key, time, params);
        }
    }

    return this->default_value && !this->default_value->is_type<nullptr_t>() ? this->default_value : nullptr;
}

float ff::key_frames::start() const
{
    return this->start_;
}

float ff::key_frames::length() const
{
    return this->length_;
}

const std::string& ff::key_frames::name() const
{
    return this->name_;
}

ff::key_frames ff::key_frames::load_from_source(std::string_view name, const ff::dict& dict, ff::resource_load_context& context)
{
    key_frames frames;
    return frames.load_from_source_internal(name, dict, context) ? frames : key_frames();
}

ff::key_frames ff::key_frames::load_from_cache(const ff::dict& dict)
{
    key_frames frames;
    return frames.load_from_cache_internal(dict) ? frames : key_frames();
}

ff::dict ff::key_frames::save_to_cache() const
{
    ff::dict dict;

    dict.set<std::string>("name", this->name_);
    dict.set_enum("method", this->method);
    dict.set<float>("start", this->start_);
    dict.set<float>("length", this->length_);
    dict.set("default", this->default_value);

    std::vector<float> frames;
    std::vector<ff::value_ptr> values;
    std::vector<ff::value_ptr> tangents;

    frames.reserve(this->keys.size());
    values.reserve(this->keys.size());
    tangents.reserve(this->keys.size());

    for (const key_frame& key : this->keys)
    {
        frames.push_back(key.frame);
        values.push_back(key.value);
        tangents.push_back(key.tangent_value);
    }

    dict.set<std::vector<float>>("frames", std::move(frames));
    dict.set<std::vector<ff::value_ptr>>("values", std::move(values));
    dict.set<std::vector<ff::value_ptr>>("tangents", std::move(tangents));

    return dict;
}

bool ff::key_frames::load_from_cache_internal(const ff::dict& dict)
{
    this->name_ = dict.get<std::string>("name");
    this->method = dict.get_enum<method_t>("method");
    this->start_ = dict.get<float>("start");
    this->length_ = dict.get<float>("length");
    this->default_value = dict.get("default");

    std::vector<float> frames = dict.get<std::vector<float>>("frames");
    std::vector<ff::value_ptr> values = dict.get<std::vector<ff::value_ptr>>("values");
    std::vector<ff::value_ptr> tangents = dict.get<std::vector<ff::value_ptr>>("tangents");

    if (frames.size() != values.size() || frames.size() != tangents.size())
    {
        assert(false);
        return false;
    }

    this->keys.reserve(frames.size());
    for (size_t i = 0; i < frames.size(); i++)
    {
        key_frame key;
        key.frame = frames[i];
        key.value = values[i];
        key.tangent_value = tangents[i];
        this->keys.push_back(std::move(key));
    }

    return true;
}

static ff::value_ptr convert_key_value(ff::value_ptr value)
{
    // Check if it's an interpolatable value

    ff::value_ptr rect_float_value = value->try_convert<ff::rect_float>();
    if (rect_float_value)
    {
        return rect_float_value;
    }

    ff::value_ptr point_float_value = value->try_convert<ff::point_float>();
    if (point_float_value)
    {
        return point_float_value;
    }

    ff::value_ptr float_value = value->try_convert<float>();
    if (float_value)
    {
        return float_value;
    }

    return value ? value : ff::value::create<nullptr_t>();
}

bool ff::key_frames::load_from_source_internal(std::string_view name, const ff::dict& dict, ff::resource_load_context& context)
{
    float tension = dict.get<float>("tension");
    tension = (1.0f - tension) / 2.0f;

    std::vector<ff::value_ptr> values = dict.get<std::vector<ff::value_ptr>>("values");
    for (ff::value_ptr value : values)
    {
        ff::dict value_dict = value->get<ff::dict>();
        ff::value_ptr frame_value = value_dict.get("frame")->try_convert<float>();
        ff::value_ptr value_value = value_dict.get("value");
        if (!frame_value || !value_value)
        {
            context.add_error("Key frame missing frame or value");
            return false;
        }

        key_frame key;
        key.frame = frame_value->get<float>();
        key.value = ::convert_key_value(value_value);
        key.tangent_value = ff::value::create<nullptr_t>();

        auto key_iter = std::lower_bound(this->keys.cbegin(), this->keys.cend(), key);
        if (key_iter != this->keys.cend() && key_iter->frame == key.frame)
        {
            std::ostringstream str;
            str << "Key frame not unique: " << key.frame;
            context.add_error(str.str());
            return false;
        }

        this->keys.insert(key_iter, key);
    }

    assert(std::is_sorted(this->keys.cbegin(), this->keys.cend()));

    this->name_ = name;
    this->start_ = dict.get<float>("start", this->keys.size() ? this->keys[0].frame : 0.0f);
    this->length_ = dict.get<float>("length", this->keys.size() ? this->keys.back().frame : 0.0f);
    this->default_value = ::convert_key_value(dict.get("default"));
    this->method = ff::key_frames::load_method(dict, false);

    if (this->length_ < 0.0f)
    {
        std::ostringstream str;
        str << "Invalid key frame length: " << this->length_;
        context.add_error(str.str());
        return false;
    }

    // Init tangents

    for (size_t i = 0; i < this->keys.size(); i++)
    {
        key_frame& key_before = this->keys[i ? i - 1 : this->keys.size() - 1];
        key_frame& key_after = this->keys[i + 1 < this->keys.size() ? i + 1 : 0];

        if (key_before.value->is_same_type(key_after.value))
        {
            if (key_before.value->is_type<float>())
            {
                float v1 = key_before.value->get<float>();
                float v2 = key_after.value->get<float>();

                this->keys[i].tangent_value = ff::value::create<float>(tension * (v2 - v1));
            }
            else if (key_before.value->is_type<ff::point_float>())
            {
                ff::point_float v1 = key_before.value->get<ff::point_float>();
                ff::point_float v2 = key_after.value->get<ff::point_float>();

                ff::rect_float output;
                DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                    DirectX::XMVectorMultiply(
                        DirectX::XMVectorReplicate(tension),
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat2(reinterpret_cast<DirectX::XMFLOAT2*>(&v2)),
                            DirectX::XMLoadFloat2(reinterpret_cast<DirectX::XMFLOAT2*>(&v1)))));

                this->keys[i].tangent_value = ff::value::create<ff::point_float>(output.top_left());
            }
            else if (key_before.value->is_type<ff::rect_float>())
            {
                ff::rect_float v1 = key_before.value->get<ff::rect_float>();
                ff::rect_float v2 = key_after.value->get<ff::rect_float>();

                ff::rect_float output;
                DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                    DirectX::XMVectorMultiply(
                        DirectX::XMVectorReplicate(tension),
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v2)),
                            DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v1)))));

                this->keys[i].tangent_value = ff::value::create<ff::rect_float>(output);
            }
        }
    }

    return true;
}

ff::value_ptr ff::key_frames::interpolate(const key_frame& lhs, const key_frame& other, float time, const ff::dict* params)
{
    ff::value_ptr value = lhs.value;

    if (value->is_type<std::string>())
    {
        const std::string& param_name = value->get<std::string>();
        if (ff::string::starts_with(param_name, "param:"))
        {
            value = params->get(param_name.substr(6));
        }
    }
    else if (lhs.value->is_same_type(other.value)) // Can only interplate the same types
    {
        // Spline interpolation
        if (lhs.tangent_value->is_same_type(lhs.value) && other.tangent_value->is_same_type(lhs.value))
        {
            if (lhs.value->is_type<float>())
            {
                // Q(s) = (2s^3 - 3s^2 + 1)v1 + (-2s^3 + 3s^2)v2 + (s^3 - 2s^2 + s)t1 + (s^3 - s^2)t2
                float time2 = time * time;
                float time3 = time2 * time;
                float v1 = lhs.value->get<float>();
                float t1 = lhs.tangent_value->get<float>();
                float v2 = other.value->get<float>();
                float t2 = other.tangent_value->get<float>();

                float output =
                    (2 * time3 - 3 * time2 + 1) * v1 +
                    (-2 * time3 + 3 * time2) * v2 +
                    (time3 - 2 * time2 + time) * t1 +
                    (time3 - time2) * t2;

                value = ff::value::create<float>(output);
            }
            else if (lhs.value->is_type<ff::point_float>())
            {
                ff::point_float v1 = lhs.value->get<ff::point_float>();
                ff::point_float t1 = lhs.tangent_value->get<ff::point_float>();
                ff::point_float v2 = other.value->get<ff::point_float>();
                ff::point_float t2 = other.tangent_value->get<ff::point_float>();

                ff::rect_float output;
                DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                    DirectX::XMVectorHermite(
                        DirectX::XMLoadFloat2(reinterpret_cast<const DirectX::XMFLOAT2*>(&v1)),
                        DirectX::XMLoadFloat2(reinterpret_cast<const DirectX::XMFLOAT2*>(&t1)),
                        DirectX::XMLoadFloat2(reinterpret_cast<const DirectX::XMFLOAT2*>(&v2)),
                        DirectX::XMLoadFloat2(reinterpret_cast<const DirectX::XMFLOAT2*>(&t2)),
                        time));

                value = ff::value::create<ff::point_float>(output.top_left());
            }
            else if (lhs.value->is_type<ff::rect_float>())
            {
                ff::rect_float v1 = lhs.value->get<ff::rect_float>();
                ff::rect_float t1 = lhs.tangent_value->get<ff::rect_float>();
                ff::rect_float v2 = other.value->get<ff::rect_float>();
                ff::rect_float t2 = other.tangent_value->get<ff::rect_float>();

                ff::rect_float output;
                DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                    DirectX::XMVectorHermite(
                        DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&v1)),
                        DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&t1)),
                        DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&v2)),
                        DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&t2)),
                        time));

                value = ff::value::create<ff::rect_float>(output);
            }
        }
        else if (lhs.value->is_type<float>())
        {
            float v1 = lhs.value->get<float>();
            float v2 = other.value->get<float>();
            float output = (v2 - v1) * time + v1;

            value = ff::value::create<float>(output);
        }
        else if (lhs.value->is_type<ff::point_float>())
        {
            ff::rect_float v1(lhs.value->get<ff::point_float>(), ff::point_float{});
            ff::rect_float v2(other.value->get<ff::point_float>(), ff::point_float{});

            ff::rect_float output;
            DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                DirectX::XMVectorLerp(
                    DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v1)),
                    DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v2)),
                    time));

            value = ff::value::create<ff::point_float>(output.top_left());
        }
        else if (lhs.value->is_type<ff::rect_float>())
        {
            ff::rect_float v1 = lhs.value->get<ff::rect_float>();
            ff::rect_float v2 = other.value->get<ff::rect_float>();

            ff::rect_float output;
            DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&output),
                DirectX::XMVectorLerp(
                    DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v1)),
                    DirectX::XMLoadFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&v2)),
                    time));

            value = ff::value::create<ff::rect_float>(output);
        }
    }

    return value;
}

ff::key_frames::method_t ff::key_frames::load_method(const ff::dict& dict, bool from_cache)
{
    method_t method = method_t::none;

    if (from_cache || dict.get("method")->is_type<int>())
    {
        method = dict.get_enum<method_t>("method");
    }
    else
    {
        std::string method_string = dict.get<std::string>("method", "linear");
        method = ff::flags::set(method, method_string == "spline" ? method_t::interpolate_spline : method_t::interpolate_linear);

        ff::value_ptr loop_value = dict.get("loop");
        if (!loop_value || loop_value->is_type<bool>())
        {
            method = ff::flags::set(method, loop_value->get<bool>() ? method_t::bounds_loop : method_t::bounds_none);
        }
        else if (loop_value->is_type<std::string>() && loop_value->get<std::string>() == "clamp")
        {
            method = ff::flags::set(method, method_t::bounds_clamp);
        }
    }

    return method;
}

bool ff::key_frames::adjust_frame(float& frame, float start, float length, ff::key_frames::method_t method)
{
    if (frame < start)
    {
        switch (ff::flags::get(method, method_t::bounds_bits))
        {
            default:
                return false;

            case method_t::bounds_clamp:
                frame = start;
                break;

            case method_t::bounds_loop:
                frame = (length != 0) ? std::fmodf(frame - start, length) + start : start;
                break;
        }
    }
    else if (frame >= start + length)
    {
        switch (ff::flags::get(method, method_t::bounds_bits))
        {
            default:
                return false;

            case method_t::bounds_clamp:
                frame = start + length;
                break;

            case method_t::bounds_loop:
                frame = (length != 0) ? std::fmodf(frame - start, length) + start : start;
                break;
        }
    }

    return true;
}

ff::create_key_frames::create_key_frames(std::string_view name, float start, float length, key_frames::method_t method, ff::value_ptr default_value)
{
    this->dict.set<std::string>("name", std::string(name));
    this->dict.set<float>("start", start);
    this->dict.set<float>("length", length);
    this->dict.set_enum("method", method);
    this->dict.set("default", default_value);
}

void ff::create_key_frames::add_frame(float frame, ff::value_ptr value)
{
    ff::dict dict;
    dict.set<float>("frame", frame);
    dict.set("value", value);

    this->values.push_back(ff::value::create<ff::dict>(std::move(dict)));
}

ff::key_frames ff::create_key_frames::create() const
{
    std::string name;
    ff::dict dict = this->create_source_dict(name);
    return key_frames::load_from_source(name, dict, ff::resource_load_context::null());
}

ff::dict ff::create_key_frames::create_source_dict(std::string& name) const
{
    name = this->dict.get<std::string>("name");

    ff::dict dict = this->dict;
    dict.set<std::vector<ff::value_ptr>>("values", std::vector<ff::value_ptr>(this->values));
    return dict;
}
