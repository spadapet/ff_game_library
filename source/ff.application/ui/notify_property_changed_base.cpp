#include "pch.h"
#include "ui/notify_property_changed_base.h"

NS_IMPLEMENT_REFLECTION(ff::ui::notify_property_changed_base)
{
    NsImpl<Noesis::INotifyPropertyChanged>();
}

Noesis::PropertyChangedEventHandler& ff::ui::notify_property_changed_base::PropertyChanged()
{
    return this->property_changed_;
}

void ff::ui::notify_property_changed_base::property_changed(std::string_view name)
{
    if (this->property_changed_)
    {
        std::string name_str(name);
        this->property_changed_.Invoke(this, Noesis::PropertyChangedEventArgs(Noesis::Symbol(name_str.c_str())));
    }
}
