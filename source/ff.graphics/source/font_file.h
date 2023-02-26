#pragma once

namespace ff
{
    class font_file : public ff::resource_file
    {
    public:
        font_file(std::shared_ptr<ff::data_base> data, size_t index, bool bold, bool italic);
        font_file(font_file&& other) noexcept = default;
        font_file(const font_file& other) = delete;

        font_file& operator=(font_file&& other) noexcept = default;
        font_file& operator=(const font_file & other) = delete;
        operator bool() const;

        bool bold() const;
        bool italic() const;
        size_t index() const;
        IDWriteFontFace5* font_face();

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        Microsoft::WRL::ComPtr<IDWriteFontFace5> font_face_;
        size_t index_;
        bool bold_;
        bool italic_;
    };
}

namespace ff::internal
{
    class font_file_factory : public ff::resource_object_factory<font_file>
    {
    public:
        using ff::resource_object_factory<font_file>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
