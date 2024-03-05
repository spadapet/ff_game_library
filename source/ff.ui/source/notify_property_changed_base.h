#pragma once

namespace ff::ui
{
    template<class T = Noesis::BaseComponent, class = std::enable_if_t<std::is_base_of_v<Noesis::BaseComponent, T>>>
    class notify_property_changed_t : public T, public Noesis::INotifyPropertyChanged
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

        template<typename... Args>
        bool properties_changed(Args&&... property_names)
        {
            std::initializer_list<std::string_view> name_list{ property_names... };
            for (std::string_view name : name_list)
            {
                this->property_changed(name);
            }
        }

    private:
        Noesis::PropertyChangedEventHandler property_changed_;

        NS_IMPLEMENT_INLINE_REFLECTION(ff::ui::notify_property_changed_t<T>, T, "notify_property_changed_t")
        {
            NsImpl<Noesis::INotifyPropertyChanged>();
        }
    };

    class notify_property_changed_base : public Noesis::BaseComponent, public Noesis::INotifyPropertyChanged
    {
    public:
        virtual Noesis::PropertyChangedEventHandler& PropertyChanged() override;

        NS_IMPLEMENT_INTERFACE_FIXUP;

    protected:
        virtual void property_changed(std::string_view name);

        template<typename... Args>
        void properties_changed(Args&&... property_names)
        {
            std::initializer_list<std::string_view> name_list{ property_names... };
            for (std::string_view name : name_list)
            {
                this->property_changed(name);
            }
        }

        template<class T, typename... Args>
        bool set_property(T& storage, const T& value, Args&&... property_names)
        {
            if (!std::equal_to<T>()(storage, value))
            {
                storage = value;

                std::initializer_list<std::string_view> name_list{ property_names... };
                for (std::string_view name : name_list)
                {
                    this->property_changed(name);
                }

                return true;
            }

            return false;
        }

    private:
        Noesis::PropertyChangedEventHandler property_changed_;

        NS_DECLARE_REFLECTION(notify_property_changed_base, Noesis::BaseComponent);
    };
}
