#pragma once

namespace ff::ui
{
    class notify_propety_changed_base : public Noesis::BaseComponent, public Noesis::INotifyPropertyChanged
    {
    public:
        virtual Noesis::PropertyChangedEventHandler& PropertyChanged() override;

        NS_IMPLEMENT_INTERFACE_FIXUP;

    protected:
        virtual void property_changed(const char* name);

    private:
        Noesis::PropertyChangedEventHandler property_changed_;

        NS_DECLARE_REFLECTION(notify_propety_changed_base, Noesis::BaseComponent);
    };
}
