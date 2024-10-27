#include "pch.h"
#include "ui/stream.h"
#include "ui/xaml_provider.h"

ff::internal::ui::xaml_provider::xaml_provider(std::shared_ptr<ff::resource_object_provider> resources)
    : resources(resources)
{}

Noesis::Ptr<Noesis::Stream> ff::internal::ui::xaml_provider::LoadXaml(const Noesis::Uri& uri)
{
    Noesis::String uri_path;
    uri.GetPath(uri_path);
    std::string_view uri_str(uri_path.Str(), uri_path.Size());

    ff::auto_resource_value res = this->resources->get_resource_object(uri_str);
    return Noesis::MakePtr<ff::internal::ui::stream>(std::move(res));
}
