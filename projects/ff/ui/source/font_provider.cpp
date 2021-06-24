#include "pch.h"
#include "font_provider.h"
#include "resource_cache.h"
#include "stream.h"
#include "ui.h"

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

    for (std::string_view name : ff::internal::ui::global_resource_cache()->resource_object_names())
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
        ff::auto_resource_value value = ff::internal::ui::global_resource_cache()->get_resource_object(name);
        if (value.valid())
        {
            return Noesis::MakePtr<ff::internal::ui::stream>(std::move(value));
        }
    }

    return nullptr;
}
