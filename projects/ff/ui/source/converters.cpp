#include "pch.h"
#include "converters.h"

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_visible_converter, "ff.ui.BoolToVisibleConverter")
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

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_collapsed_converter, "ff.ui.BoolToCollapsedConverter")
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

NS_IMPLEMENT_REFLECTION(ff::ui::bool_to_object_converter, "ff.BoolToObjectConverter")
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
