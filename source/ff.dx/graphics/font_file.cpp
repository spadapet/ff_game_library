#include "pch.h"
#include "graphics/font_file.h"
#include "write/write.h"

ff::font_file::font_file(std::shared_ptr<ff::data_base> data, size_t index, bool bold, bool italic)
    : ff::resource_file(std::make_shared<ff::saved_data_static>(data, data->size(), ff::saved_data_type::none), ".ttf")
    , index_(index)
    , bold_(bold)
    , italic_(italic)
{
    if (data)
    {
        DWRITE_FONT_SIMULATIONS font_flags =
            (this->bold_ ? DWRITE_FONT_SIMULATIONS_BOLD : DWRITE_FONT_SIMULATIONS_NONE) |
            (this->italic_ ? DWRITE_FONT_SIMULATIONS_OBLIQUE : DWRITE_FONT_SIMULATIONS_NONE);

        Microsoft::WRL::ComPtr<IDWriteFontFile> font_file;
        Microsoft::WRL::ComPtr<IDWriteFontFaceReference> font_face_ref;
        Microsoft::WRL::ComPtr<IDWriteFontFace3> font_face;

        if (FAILED(ff::write_font_loader()->CreateInMemoryFontFileReference(ff::write_factory(), data->data(), static_cast<UINT32>(data->size()), nullptr, font_file.GetAddressOf())) ||
            FAILED(ff::write_factory()->CreateFontFaceReference(font_file.Get(), static_cast<UINT32>(this->index_), font_flags, font_face_ref.GetAddressOf())) ||
            FAILED(font_face_ref->CreateFontFace(font_face.GetAddressOf())) ||
            FAILED(font_face.As(&this->font_face_)))
        {
            assert(false);
        }
    }
}

ff::font_file::operator bool() const
{
    return this->font_face_;
}

bool ff::font_file::bold() const
{
    return this->bold_;
}

bool ff::font_file::italic() const
{
    return this->italic_;
}

size_t ff::font_file::index() const
{
    return this->index_;
}

IDWriteFontFace5* ff::font_file::font_face()
{
    return this->font_face_.Get();
}

bool ff::font_file::save_to_cache(ff::dict& dict) const
{
    if (ff::resource_file::save_to_cache(dict))
    {
        dict.set<size_t>("index", this->index_);
        dict.set<bool>("bold", this->bold_);
        dict.set<bool>("italic", this->italic_);
        return true;
    }

    return false;
}

std::shared_ptr<ff::resource_object_base> ff::internal::font_file_factory::load_from_source(const ff::dict& dict, resource_load_context& context) const
{
    size_t index = dict.get<size_t>("index");
    bool bold = dict.get<bool>("bold");
    bool italic = dict.get<bool>("italic");

    std::filesystem::path file_path = dict.get<std::string>("file");
    std::shared_ptr<ff::data_mem_mapped> data = std::make_shared<ff::data_mem_mapped>(file_path);
    return data->valid() ? std::make_shared<ff::font_file>(data, index, bold, italic) : nullptr;
}

std::shared_ptr<ff::resource_object_base> ff::internal::font_file_factory::load_from_cache(const ff::dict& dict) const
{
    const ff::resource_object_factory_base* resource_file_factory = ff::resource_object_base::get_factory(typeid(ff::resource_file));
    if (resource_file_factory)
    {
        std::shared_ptr<ff::resource_file> file = std::dynamic_pointer_cast<ff::resource_file>(resource_file_factory->load_from_cache(dict));
        if (file)
        {
            size_t index = dict.get<size_t>("index");
            bool bold = dict.get<bool>("bold");
            bool italic = dict.get<bool>("italic");

            return std::make_shared<ff::font_file>(file->loaded_data(), index, bold, italic);
        }
    }

    return nullptr;
}
