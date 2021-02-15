#pragma once

namespace ff
{
    class font_data : public ff::resource_file
    {
    public:
        font_data(std::shared_ptr<ff::data_base> data, size_t index, bool bold, bool italic);
        font_data(font_data&& other) noexcept = default;
        font_data(const font_data& other) = delete;

        font_data& operator=(font_data&& other) noexcept = default;
        font_data& operator=(const font_data & other) = delete;
        operator bool() const;

        bool bold() const;
        bool italic() const;
        size_t index() const;
        IDWriteFontFaceX* font_face();

    protected:
        virtual bool save_to_cache(ff::dict& dict, bool& allow_compress) const override;

    private:
        Microsoft::WRL::ComPtr<IDWriteFontFaceX> font_face_;
        size_t index_;
        bool bold_;
        bool italic_;
    };
}

namespace ff::internal
{
    class font_data_factory : public ff::resource_object_factory<font_data>
    {
    public:
        using ff::resource_object_factory<font_data>::resource_object_factory;

        virtual std::shared_ptr<resource_object_base> load_from_source(const ff::dict& dict, resource_load_context& context) const override;
        virtual std::shared_ptr<resource_object_base> load_from_cache(const ff::dict& dict) const override;
    };
}
