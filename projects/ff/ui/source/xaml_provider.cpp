#include "pch.h"
#include "resource_cache.h"
#include "stream.h"
#include "ui.h"
#include "xaml_provider.h"

Noesis::Ptr<Noesis::Stream> ff::internal::ui::xaml_provider::LoadXaml(const char* uri)
{
    ff::auto_resource_value res = ff::internal::ui::global_resource_cache()->get_resource_object(std::string_view(uri));
    return Noesis::MakePtr<ff::internal::ui::stream>(std::move(res));
}
