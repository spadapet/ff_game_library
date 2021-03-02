#pragma once

namespace ff::internal::ui
{
    class font_provider : public Noesis::CachedFontProvider
    {
    public:
        virtual void ScanFolder(const char* folder) override;
        virtual Noesis::Ptr<Noesis::Stream> OpenFont(const char* folder, const char* filename) const override;
    };
}
