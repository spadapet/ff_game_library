#include "pch.h"
#include "resource_load_context.h"

ff::internal::resource_load_context::resource_load_context(std::string_view base_path, bool debug)
    : base_path_data(base_path)
    , debug_data(debug)
{}

const std::filesystem::path& ff::internal::resource_load_context::base_path() const
{
    return this->base_path_data;
}

const std::vector<std::string> ff::internal::resource_load_context::errors() const
{
    return this->errors_data;
}

void ff::internal::resource_load_context::add_error(std::string_view text)
{
    if (!text.empty())
    {
        this->errors_data.emplace_back(text);
    }
}

bool ff::internal::resource_load_context::debug() const
{
    return this->debug_data;
}
