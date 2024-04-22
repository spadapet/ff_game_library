#include "pch.h"
#include "font_provider.h"
#include "stream.h"

ff::internal::ui::font_provider::font_provider(std::shared_ptr<ff::resource_object_provider> resources)
    : resources(resources)
{}

void ff::internal::ui::font_provider::ScanFolder(const Noesis::Uri& folder)
{
    Noesis::String folder_path;
    folder.GetPath(folder_path);

    std::string prefix("#");
    if (folder_path.Size())
    {
        prefix += folder_path.Str();
        prefix += "/";
    }

    for (std::string_view name : this->resources->resource_object_names())
    {
        if (name.size() > prefix.size() && !::_strnicmp(name.data(), prefix.c_str(), prefix.size()))
        {
            this->RegisterFont(folder, std::string(name).c_str());
        }
    }
}

Noesis::Ptr<Noesis::Stream> ff::internal::ui::font_provider::OpenFont(const Noesis::Uri& folder, const char* filename) const
{
    std::string_view name(filename);
    if (name.size() > 0 && name[0] == L'#')
    {
        ff::auto_resource_value value = this->resources->get_resource_object(name);
        return Noesis::MakePtr<ff::internal::ui::stream>(std::move(value));
    }

    return nullptr;
}
