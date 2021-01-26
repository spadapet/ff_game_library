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
    assertRetVal(frames.size() == values.size() && frames.size() == tangents.size(), false);

    this->keys.Reserve(frames.size());
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

static ff::value_ptr ConvertKeyValue(ff::value_ptr value)
{
    // Check if it's an interpolatable value

    ff::ValuePtrT<ff::RectFloatValue> rectFloatValue = value;
    if (rectFloatValue)
    {
        return rectFloatValue;
    }

    ff::ValuePtrT<ff::PointFloatValue> pointFloatValue = value;
    if (pointFloatValue)
    {
        return pointFloatValue;
    }

    ff::ValuePtrT<float> floatValue = value;
    if (floatValue)
    {
        return floatValue;
    }

    return value ? value : ff::Value::New<ff::NullValue>();
}

bool ff::key_frames::load_from_source_internal(ff::StringRef name, const ff::dict& dict, ff::IResourceLoadListener* loadListener)
{
    float tension = dict.get<float>(::PROP_TENSION, 0.0f);
    tension = (1.0f - tension) / 2.0f;

    std::vector<ff::value_ptr> values = dict.get<std::vector<ff::value_ptr>>("values");
    for (ff::value_ptr value : values)
    {
        ff::dict valueDict = value->get_value<ff::DictValue>();
        ff::ValuePtrT<float> frameValue = valueDict.get_value(::PROP_FRAME);
        ff::value_ptr valueValue = valueDict.get_value(::PROP_VALUE);
        if (!frameValue || !valueValue)
        {
            if (loadListener)
            {
                loadListener->AddError(ff::String::from_static(L"Key frame missing frame or value"));
            }

            assertRetVal(false, false);
        }

        key_frame key;
        key.frame = frameValue.get_value();
        key.value = ::ConvertKeyValue(valueValue);
        key.tangent_value = ff::Value::New<ff::NullValue>();

        size_t keyPos;
        if (this->keys.SortFind(key, &keyPos))
        {
            if (loadListener)
            {
                loadListener->AddError(ff::String::format_new(L"Key frame not unique: %g", key.frame));
            }

            assertRetVal(false, false);
        }

        this->keys.Insert(keyPos, key);
    }

    this->name_ = name;
    this->start_ = dict.get<float>("start", this->keys.size() ? this->keys[0].frame : 0.0f);
    this->length_ = dict.get<float>("length", this->keys.size() ? this->keys.GetLast().frame : 0.0f);
    this->default_value = ::ConvertKeyValue(dict.get_value("default"));
    this->method = load_method(dict, false);

    if (this->length_ < 0.0f)
    {
        if (loadListener)
        {
            loadListener->AddError(ff::String::format_new(L"Invalid key frame length: %g", this->length_));
        }

        assertRetVal(false, false);
    }

    // Init tangents

    for (size_t i = 0; i < this->keys.size(); i++)
    {
        key_frame& keyBefore = this->keys[i ? i - 1 : this->keys.size() - 1];
        key_frame& keyAfter = this->keys[i + 1 < this->keys.size() ? i + 1 : 0];

        if (keyBefore.value->IsSameType(keyAfter.value))
        {
            if (keyBefore.value->is_type<float>())
            {
                float v1 = keyBefore.value->get_value<float>();
                float v2 = keyAfter.value->get_value<float>();

                this->keys[i].tangent_value = ff::Value::New<float>(tension * (v2 - v1));
            }
            else if (keyBefore.value->is_type<ff::PointFloatValue>())
            {
                ff::PointFloat v1 = keyBefore.value->get_value<ff::PointFloatValue>();
                ff::PointFloat v2 = keyAfter.value->get_value<ff::PointFloatValue>();

                ff::RectFloat output;
                DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                    DirectX::XMVectorMultiply(
                        DirectX::XMVectorReplicate(tension),
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat2((DirectX::XMFLOAT2*)&v2),
                            DirectX::XMLoadFloat2((DirectX::XMFLOAT2*)&v1))));

                this->keys[i].tangent_value = ff::Value::New<ff::PointFloatValue>(output.TopLeft());
            }
            else if (keyBefore.value->is_type<ff::RectFloatValue>())
            {
                ff::RectFloat v1 = keyBefore.value->get_value<ff::RectFloatValue>();
                ff::RectFloat v2 = keyAfter.value->get_value<ff::RectFloatValue>();

                ff::RectFloat output;
                DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                    DirectX::XMVectorMultiply(
                        DirectX::XMVectorReplicate(tension),
                        DirectX::XMVectorSubtract(
                            DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v2),
                            DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v1))));

                this->keys[i].tangent_value = ff::Value::New<ff::RectFloatValue>(output);
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
        ff::String paramName = value->get_value<std::string>();
        if (!std::wcsncmp(paramName.c_str(), L"param:", 6))
        {
            ff::StaticString paramName2(paramName.c_str() + 6, paramName.size() - 6);
            value = params->get_value(paramName2);
        }
    }
    else if (lhs.value->IsSameType(other.value)) // Can only interplate the same types
    {
        // Spline interpolation
        if (lhs.tangent_value->IsSameType(lhs.value) && other.tangent_value->IsSameType(lhs.value))
        {
            if (lhs.value->is_type<float>())
            {
                // Q(s) = (2s^3 - 3s^2 + 1)v1 + (-2s^3 + 3s^2)v2 + (s^3 - 2s^2 + s)t1 + (s^3 - s^2)t2
                float time2 = time * time;
                float time3 = time2 * time;
                float v1 = lhs.value->get_value<float>();
                float t1 = lhs.tangent_value->get_value<float>();
                float v2 = other.value->get_value<float>();
                float t2 = other.tangent_value->get_value<float>();

                float output =
                    (2 * time3 - 3 * time2 + 1) * v1 +
                    (-2 * time3 + 3 * time2) * v2 +
                    (time3 - 2 * time2 + time) * t1 +
                    (time3 - time2) * t2;

                value = ff::Value::New<float>(output);
            }
            else if (lhs.value->is_type<ff::PointFloatValue>())
            {
                ff::PointFloat v1 = lhs.value->get_value<ff::PointFloatValue>();
                ff::PointFloat t1 = lhs.tangent_value->get_value<ff::PointFloatValue>();
                ff::PointFloat v2 = other.value->get_value<ff::PointFloatValue>();
                ff::PointFloat t2 = other.tangent_value->get_value<ff::PointFloatValue>();

                ff::RectFloat output;
                DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                    DirectX::XMVectorHermite(
                        DirectX::XMLoadFloat2((const DirectX::XMFLOAT2*)&v1),
                        DirectX::XMLoadFloat2((const DirectX::XMFLOAT2*)&t1),
                        DirectX::XMLoadFloat2((const DirectX::XMFLOAT2*)&v2),
                        DirectX::XMLoadFloat2((const DirectX::XMFLOAT2*)&t2),
                        time));

                value = ff::Value::New<ff::PointFloatValue>(output.TopLeft());
            }
            else if (lhs.value->is_type<ff::RectFloatValue>())
            {
                ff::RectFloat v1 = lhs.value->get_value<ff::RectFloatValue>();
                ff::RectFloat t1 = lhs.tangent_value->get_value<ff::RectFloatValue>();
                ff::RectFloat v2 = other.value->get_value<ff::RectFloatValue>();
                ff::RectFloat t2 = other.tangent_value->get_value<ff::RectFloatValue>();

                ff::RectFloat output;
                DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                    DirectX::XMVectorHermite(
                        DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&v1),
                        DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&t1),
                        DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&v2),
                        DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&t2),
                        time));

                value = ff::Value::New<ff::RectFloatValue>(output);
            }
        }
        else if (lhs.value->is_type<float>())
        {
            float v1 = lhs.value->get_value<float>();
            float v2 = other.value->get_value<float>();
            float output = (v2 - v1) * time + v1;

            value = ff::Value::New<float>(output);
        }
        else if (lhs.value->is_type<ff::PointFloatValue>())
        {
            ff::RectFloat v1(lhs.value->get_value<ff::PointFloatValue>(), ff::PointFloat::Zeros());
            ff::RectFloat v2(other.value->get_value<ff::PointFloatValue>(), ff::PointFloat::Zeros());

            ff::RectFloat output;
            DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                DirectX::XMVectorLerp(
                    DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v1),
                    DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v2),
                    time));

            value = ff::Value::New<ff::PointFloatValue>(output.TopLeft());
        }
        else if (lhs.value->is_type<ff::RectFloatValue>())
        {
            ff::RectFloat v1 = lhs.value->get_value<ff::RectFloatValue>();
            ff::RectFloat v2 = other.value->get_value<ff::RectFloatValue>();

            ff::RectFloat output;
            DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&output,
                DirectX::XMVectorLerp(
                    DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v1),
                    DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&v2),
                    time));

            value = ff::Value::New<ff::RectFloatValue>(output);
        }
    }

    return value;
}

ff::key_frames::method_t ff::key_frames::load_method(const ff::dict& dict, bool fromCache)
{
    method_t method = method_t::none;

    if (fromCache || dict.get_value("method")->is_type<int>())
    {
        method = (method_t)dict.get<int>("method");
    }
    else
    {
        ff::String methodString = dict.get<std::string>("method", ff::String::from_static(L"linear"));
        method = ff::SetFlags(method, methodString == L"spline" ? method_t::interpolate_spline : method_t::interpolate_linear);

        ff::value_ptr loopValue = dict.get_value(::PROP_LOOP);
        if (!loopValue || loopValue->is_type<ff::BoolValue>())
        {
            method = ff::SetFlags(method, loopValue->get_value<ff::BoolValue>() ? method_t::bounds_loop : method_t::bounds_none);
        }
        else if (loopValue->is_type<std::string>() && loopValue->get_value<std::string>() == L"clamp")
        {
            method = ff::SetFlags(method, method_t::bounds_clamp);
        }
    }

    return method;
}

bool ff::key_frames::adjust_frame(float& frame, float start, float length, ff::key_frames::method_t method)
{
    if (frame < start)
    {
        switch (ff::GetFlags(method, method_t::bounds_bits))
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
        switch (ff::GetFlags(method, method_t::bounds_bits))
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

ff::create_key_frames::create_key_frames(ff::StringRef name, float start, float length, key_frames::method_t method, ff::value_ptr defaultValue)
{
    _dict.Set<std::string>("name", name);
    _dict.Set<float>("start", start);
    _dict.Set<float>("length", length);
    _dict.Set<int>("method", (int)method);
    _dict.SetValue("default", defaultValue);
}

ff::create_key_frames::create_key_frames(const create_key_frames& other)
    : _dict(other._dict)
    , _values(other._values)
{}

ff::create_key_frames::create_key_frames(create_key_frames&& other)
    : _dict(std::move(other._dict))
    , _values(std::move(other._values))
{}

void ff::create_key_frames::add_frame(float frame, ff::value_ptr value)
{
    ff::dict dict;
    dict.Set<float>(::PROP_FRAME, frame);
    dict.SetValue(::PROP_VALUE, value);

    _values.push_back(ff::Value::New<ff::DictValue>(std::move(dict)));
}

ff::key_frames ff::create_key_frames::create() const
{
    ff::String name;
    ff::dict dict = create_source_dict(name);
    return key_frames::load_from_source(name, dict);
}

ff::dict ff::create_key_frames::create_source_dict(ff::StringOut name) const
{
    name = _dict.get<std::string>("name");

    ff::dict dict = _dict;
    dict.Set<std::vector<ff::value_ptr>>("values", std::vector<ff::value_ptr>(_values));
    return dict;
}
