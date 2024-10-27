#include "pch.h"
#include "ui/converters.h"

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_visible_converter, "ff.bool_to_visible_converter")
{}

bool ff::ui::bool_to_visible_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    if (Noesis::Boxing::CanUnbox<bool>(value))
    {
        bool visible = Noesis::Boxing::Unbox<bool>(value);
        result = Noesis::Boxing::Box<Noesis::Visibility>(visible ? Noesis::Visibility_Visible : Noesis::Visibility_Collapsed);
        return true;
    }

    return false;
}

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_collapsed_converter, "ff.bool_to_collapsed_converter")
{}

bool ff::ui::bool_to_collapsed_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    if (Noesis::Boxing::CanUnbox<bool>(value))
    {
        bool visible = !Noesis::Boxing::Unbox<bool>(value);
        result = Noesis::Boxing::Box<Noesis::Visibility>(visible ? Noesis::Visibility_Visible : Noesis::Visibility_Collapsed);
        return true;
    }

    return false;
}

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_inverse_converter, "ff.bool_to_inverse_converter")
{}

bool ff::ui::bool_to_inverse_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    if (Noesis::Boxing::CanUnbox<bool>(value))
    {
        bool inverse = !Noesis::Boxing::Unbox<bool>(value);
        result = Noesis::Boxing::Box(inverse);
        return true;
    }

    return false;
}

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_object_converter, "ff.bool_to_object_converter")
{
    NsProp("TrueValue", &bool_to_object_converter::true_value, &bool_to_object_converter::true_value);
    NsProp("FalseValue", &bool_to_object_converter::false_value, &bool_to_object_converter::false_value);
}

bool ff::ui::bool_to_object_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    if (Noesis::Boxing::CanUnbox<bool>(value))
    {
        bool value2 = Noesis::Boxing::Unbox<bool>(value);
        result = value2 ? this->true_value_ : this->false_value_;
        return true;
    }

    return false;
}

Noesis::BaseComponent* ff::ui::bool_to_object_converter::true_value() const
{
    return this->true_value_;
}

void ff::ui::bool_to_object_converter::true_value(Noesis::BaseComponent* value)
{
    this->true_value_.Reset(value);
}

Noesis::BaseComponent* ff::ui::bool_to_object_converter::false_value() const
{
    return this->false_value_;
}

void ff::ui::bool_to_object_converter::false_value(Noesis::BaseComponent* value)
{
    this->false_value_.Reset(value);
}

NS_IMPLEMENT_REFLECTION(ff::ui::object_to_visible_converter, "ff.object_to_visible_converter")
{}

bool ff::ui::object_to_visible_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    result = Noesis::Boxing::Box<Noesis::Visibility>(value ? Noesis::Visibility_Visible : Noesis::Visibility_Collapsed);
    return true;
}

NS_IMPLEMENT_REFLECTION(ff::ui::object_to_collapsed_converter, "ff.object_to_collapsed_converter")
{}

bool ff::ui::object_to_collapsed_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    result = Noesis::Boxing::Box<Noesis::Visibility>(value ? Noesis::Visibility_Collapsed : Noesis::Visibility_Visible);
    return true;
}

NS_IMPLEMENT_REFLECTION(ff::ui::object_to_object_converter, "ff.object_to_object_converter")
{
    NsProp("NullValue", &object_to_object_converter::null_value, &object_to_object_converter::null_value);
    NsProp("NonNullValue", &object_to_object_converter::non_null_value, &object_to_object_converter::non_null_value);
}

bool ff::ui::object_to_object_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    result = value ? this->non_null_value_ : this->null_value_;
    return true;
}

Noesis::BaseComponent* ff::ui::object_to_object_converter::null_value() const
{
    return this->null_value_;
}

void ff::ui::object_to_object_converter::null_value(Noesis::BaseComponent* value)
{
    this->null_value_.Reset(value);
}

Noesis::BaseComponent* ff::ui::object_to_object_converter::non_null_value() const
{
    return this->non_null_value_;
}

void ff::ui::object_to_object_converter::non_null_value(Noesis::BaseComponent* value)
{
    this->non_null_value_.Reset(value);
}

NS_IMPLEMENT_REFLECTION(ff::ui::level_to_indent_converter, "ff.level_to_indent_converter")
{}

bool ff::ui::level_to_indent_converter::TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result)
{
    double indent = Noesis::Boxing::CanUnbox<double>(parameter) ? Noesis::Boxing::Unbox<double>(parameter) : 8.0;
    int level = Noesis::Boxing::CanUnbox<int>(value) ? Noesis::Boxing::Unbox<int>(value) : 0;
    result = Noesis::Boxing::Box<Noesis::Thickness>(Noesis::Thickness(static_cast<float>(level * indent), 0, 0, 0));
    return true;
}
