#pragma once

namespace ff::ui
{
    template<class T = Noesis::BaseComponent, class = std::enable_if_t<std::is_base_of_v<Noesis::BaseComponent, T>>>
    class notify_propety_changed_t : public T, public Noesis::INotifyPropertyChanged
    {
    public:
        virtual Noesis::PropertyChangedEventHandler& PropertyChanged() override
        {
            return this->property_changed_;
        }

        NS_IMPLEMENT_INTERFACE_FIXUP;

    protected:
        virtual void property_changed(const char* name)
        {
            if (this->property_changed_)
            {
                this->property_changed_.Invoke(this, Noesis::PropertyChangedEventArgs(Noesis::Symbol(name)));
            }
        }

    private:
        Noesis::PropertyChangedEventHandler property_changed_;

        NS_IMPLEMENT_INLINE_REFLECTION(ff::ui::notify_propety_changed_t<T>, T, "notify_propety_changed_t")
        {
            NsImpl<Noesis::INotifyPropertyChanged>();
        }
    };

    class notify_propety_changed_base : public Noesis::BaseComponent, public Noesis::INotifyPropertyChanged
    {
    public:
        virtual Noesis::PropertyChangedEventHandler& PropertyChanged() override;

        NS_IMPLEMENT_INTERFACE_FIXUP;

    protected:
        virtual void property_changed(const char* name);

        template<class T>
        bool set_property(T& storage, const T& value, const char* name)
        {
            if (!std::equal_to<T>()(storage, value))
            {
                storage = value;
                this->property_changed(name);
                return true;
            }

            return false;
        }

    private:
        Noesis::PropertyChangedEventHandler property_changed_;

        NS_DECLARE_REFLECTION(notify_propety_changed_base, Noesis::BaseComponent);
    };
}
