#pragma once

namespace ff::internal::ui
{
    class resource_cache;

    class font_provider : public Noesis::CachedFontProvider
    {
    public:
        font_provider(std::shared_ptr<ff::resource_object_provider> resources);

        virtual void ScanFolder(const Noesis::Uri& folder) override;
        virtual Noesis::Ptr<Noesis::Stream> OpenFont(const Noesis::Uri& folder, const char* filename) const override;

    private:
        std::shared_ptr<ff::resource_object_provider> resources;
    };
}
