#pragma once

namespace ff::ui
{
    class bool_to_visible_converter : public Noesis::BaseValueConverter
    {
    public:
        virtual bool TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result) override;

    private:
        NS_DECLARE_REFLECTION(bool_to_visible_converter, Noesis::BaseValueConverter);
    };

    class bool_to_collapsed_converter : public Noesis::BaseValueConverter
    {
    public:
        virtual bool TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result) override;

    private:
        NS_DECLARE_REFLECTION(bool_to_collapsed_converter, Noesis::BaseValueConverter);
    };

    class bool_to_object_converter : public Noesis::BaseValueConverter
    {
    public:
        virtual bool TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result) override;

        Noesis::BaseComponent* true_value() const;
        void true_value(Noesis::BaseComponent* value);
        Noesis::BaseComponent* false_value() const;
        void false_value(Noesis::BaseComponent* value);

    private:
        Noesis::Ptr<Noesis::BaseComponent> true_value_;
        Noesis::Ptr<Noesis::BaseComponent> false_value_;

        NS_DECLARE_REFLECTION(bool_to_object_converter, Noesis::BaseValueConverter);
    };

    class object_to_object_converter : public Noesis::BaseValueConverter
    {
    public:
        virtual bool TryConvert(Noesis::BaseComponent* value, const Noesis::Type* targetType, Noesis::BaseComponent* parameter, Noesis::Ptr<Noesis::BaseComponent>& result) override;

        Noesis::BaseComponent* null_value() const;
        void null_value(Noesis::BaseComponent* value);
        Noesis::BaseComponent* non_null_value() const;
        void non_null_value(Noesis::BaseComponent* value);

    private:
        Noesis::Ptr<Noesis::BaseComponent> null_value_;
        Noesis::Ptr<Noesis::BaseComponent> non_null_value_;

        NS_DECLARE_REFLECTION(object_to_object_converter, Noesis::BaseValueConverter);
    };
}
