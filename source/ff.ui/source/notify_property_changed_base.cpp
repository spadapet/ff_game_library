#include "pch.h"
#include "notify_property_changed_base.h"

NS_IMPLEMENT_REFLECTION(ff::ui::notify_propety_changed_base)
{
    NsImpl<Noesis::INotifyPropertyChanged>();
}

Noesis::PropertyChangedEventHandler& ff::ui::notify_propety_changed_base::PropertyChanged()
{
    return this->property_changed_;
}

void ff::ui::notify_propety_changed_base::property_changed(const char* name)
{
    if (this->property_changed_)
    {
        this->property_changed_.Invoke(this, Noesis::PropertyChangedEventArgs(Noesis::Symbol(name)));
    }
}
