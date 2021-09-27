#pragma once

namespace ff::internal::ui
{
    class font_provider : public Noesis::CachedFontProvider
    {
    public:
        virtual void ScanFolder(const Noesis::Uri& folder) override;
        virtual Noesis::Ptr<Noesis::Stream> OpenFont(const Noesis::Uri& folder, const char* filename) const override;
    };
}
