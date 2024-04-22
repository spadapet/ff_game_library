#pragma once

namespace ff::internal::ui
{
    class xaml_provider : public Noesis::XamlProvider
    {
    public:
        xaml_provider(std::shared_ptr<ff::resource_object_provider> resources);

        virtual Noesis::Ptr<Noesis::Stream> LoadXaml(const Noesis::Uri& uri) override;

    private:
        std::shared_ptr<ff::resource_object_provider> resources;
    };
}
