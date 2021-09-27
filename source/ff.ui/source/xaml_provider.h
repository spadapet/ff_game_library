#pragma once

namespace ff::internal::ui
{
    class xaml_provider : public Noesis::XamlProvider
    {
    public:
        virtual Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override;
    };
}
