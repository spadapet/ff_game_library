#include "pch.h"
#include "resource_cache.h"
#include "stream.h"
#include "ui.h"
#include "xaml_provider.h"

Noesis::Ptr<Noesis::Stream> ff::internal::ui::xaml_provider::LoadXaml(const Noesis::Uri& uri)
{
    Noesis::String uri_path;
    uri.GetPath(uri_path);
    std::string_view uri_str(uri_path.Str(), uri_path.Size());

    ff::auto_resource_value res = ff::internal::ui::global_resource_cache()->get_resource_object(uri_str);
    return Noesis::MakePtr<ff::internal::ui::stream>(std::move(res));
}
